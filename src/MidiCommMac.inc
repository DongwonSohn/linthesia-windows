// Native macOS MIDI transport and the built-in General MIDI synthesizer.
// Included by MidiComm.cpp only on macOS. See COPYING for license information.

#include "MidiComm.h"

#include <AudioToolbox/AudioToolbox.h>
#include <CoreMIDI/CoreMIDI.h>
#include <CoreFoundation/CoreFoundation.h>
#include <atomic>
#include <cstdio>
#include <mutex>
#include <stdexcept>

struct MidiCommInMacState {
  MIDIPortRef port = 0;
  MIDIEndpointRef source = 0;
  std::queue<MidiEvent> events;
  std::mutex mutex;
  unsigned char status = 0;
  unsigned char data[2] = {0, 0};
  unsigned int length = 0;
  unsigned long generation = 0;
};

namespace {
bool midi_started = false;
MIDIClientRef midi_client = 0;
MIDIPortRef midi_output = 0;
AUGraph synth_graph = 0;
AudioUnit synth_unit = 0;
std::atomic<unsigned long> device_generation(0);
std::mutex keyboard_mutex;
MidiCommInMacState *keyboard_input = 0;

void CheckAudio(OSStatus status, const char *operation) {
  if (status != noErr)
    throw std::runtime_error(std::string(operation) + " failed (" +
                             std::to_string(status) + ")");
}

void DeviceNotification(const MIDINotification *, void *) {
  ++device_generation;
}

std::string DeviceName(MIDIObjectRef object) {
  CFStringRef name = 0;
  if (MIDIObjectGetStringProperty(object, kMIDIPropertyDisplayName, &name) != noErr || !name)
    return "MIDI device";
  char text[1024] = {0};
  CFStringGetCString(name, text, sizeof(text), kCFStringEncodingUTF8);
  CFRelease(name);
  return text;
}

MidiCommDescription Describe(MIDIEndpointRef endpoint, unsigned int id) {
  MidiCommDescription result;
  result.id = id;
  result.name = DeviceName(endpoint);
  SInt32 unique_id = 0;
  MIDIObjectGetIntegerProperty(endpoint, kMIDIPropertyUniqueID, &unique_id);
  result.client = unique_id;
  result.port = static_cast<int>(endpoint);
  return result;
}

void ReadPackets(const MIDIPacketList *packets, void *context, void *) {
  MidiCommInMacState *state = static_cast<MidiCommInMacState *>(context);
  std::lock_guard<std::mutex> lock(state->mutex);
  const MIDIPacket *packet = &packets->packet[0];
  for (UInt32 p = 0; p < packets->numPackets; ++p) {
    for (UInt16 b = 0; b < packet->length; ++b) {
      unsigned char value = packet->data[b];
      if (value >= 0xF8) // MIDI real-time messages may occur inside any message.
        continue;
      if (value & 0x80) {
        state->status = value < 0xF0 ? value : 0;
        state->length = 0;
        continue;
      }
      if (!state->status)
        continue;
      state->data[state->length++] = value;
      unsigned char type = state->status & 0xF0;
      unsigned int expected = (type == 0xC0 || type == 0xD0) ? 1 : 2;
      if (state->length == expected) {
        MidiEventSimple event(state->status, state->data[0], expected == 2 ? state->data[1] : 0);
        if (type == 0x90 && event.byte2 == 0)
          event.status = 0x80 | (event.status & 0x0F);
        state->events.push(MidiEvent::Build(event));
        state->length = 0; // Preserve running status for the next message.
      }
    }
    packet = MIDIPacketNext(packet);
  }
}

void OutputEvent(const MidiCommDescription &device, unsigned char status,
                 unsigned char data1, unsigned char data2) {
  if (device.port == 0) {
    if (synth_unit)
      MusicDeviceMIDIEvent(synth_unit, status, data1, data2, 0);
    return;
  }
  if (!midi_output)
    return;
  unsigned char data[3] = {status, data1, data2};
  UInt16 length = ((status & 0xF0) == 0xC0 || (status & 0xF0) == 0xD0) ? 2 : 3;
  MIDIPacketList packets;
  MIDIPacket *packet = MIDIPacketListInit(&packets);
  MIDIPacketListAdd(&packets, sizeof(packets), packet, 0, length, data);
  MIDISend(midi_output, static_cast<MIDIEndpointRef>(device.port), &packets);
}
}

void midiInit() {
  if (midi_started)
    return;
  midi_started = true;
  OSStatus status = MIDIClientCreate(CFSTR("Linthesia"), DeviceNotification, 0, &midi_client);
  if (status == noErr)
    MIDIOutputPortCreate(midi_client, CFSTR("Linthesia Output"), &midi_output);
  else
    std::fprintf(stderr, "CoreMIDI client unavailable (%d); computer keyboard still works.\n", int(status));

  try {
    CheckAudio(NewAUGraph(&synth_graph), "NewAUGraph");
    AudioComponentDescription description = {};
    description.componentType = kAudioUnitType_MusicDevice;
    description.componentSubType = kAudioUnitSubType_DLSSynth;
    description.componentManufacturer = kAudioUnitManufacturer_Apple;
    AUNode synth_node = 0, output_node = 0;
    CheckAudio(AUGraphAddNode(synth_graph, &description, &synth_node), "Add DLS synthesizer");
    description.componentType = kAudioUnitType_Output;
    description.componentSubType = kAudioUnitSubType_DefaultOutput;
    CheckAudio(AUGraphAddNode(synth_graph, &description, &output_node), "Add audio output");
    CheckAudio(AUGraphOpen(synth_graph), "Open audio graph");
    CheckAudio(AUGraphNodeInfo(synth_graph, synth_node, 0, &synth_unit), "Find synthesizer");
    CheckAudio(AUGraphConnectNodeInput(synth_graph, synth_node, 0, output_node, 0), "Connect synthesizer");
    CheckAudio(AUGraphInitialize(synth_graph), "Initialize audio graph");
    CheckAudio(AUGraphStart(synth_graph), "Start audio graph");
    std::fprintf(stdout, "macOS General MIDI synthesizer ready.\n");
  } catch (const std::exception &error) {
    std::fprintf(stderr, "Built-in MIDI sound unavailable: %s\n", error.what());
    if (synth_graph)
      DisposeAUGraph(synth_graph);
    synth_graph = 0;
    synth_unit = 0;
  }
}

void midiStop() {
  if (synth_graph) {
    AUGraphStop(synth_graph);
    DisposeAUGraph(synth_graph);
  }
  synth_graph = 0;
  synth_unit = 0;
  if (midi_client)
    MIDIClientDispose(midi_client);
  midi_client = 0;
  midi_output = 0;
  midi_started = false;
}

void sendNote(const unsigned char note, bool on) {
  std::lock_guard<std::mutex> keyboard_lock(keyboard_mutex);
  if (!keyboard_input)
    return;
  std::lock_guard<std::mutex> lock(keyboard_input->mutex);
  keyboard_input->events.push(MidiEvent::Build(MidiEventSimple(on ? 0x90 : 0x80, note, on ? 60 : 0)));
}

MidiCommDescriptionList MidiCommIn::GetDeviceList() {
  midiInit();
  MidiCommDescriptionList result;
  MidiCommDescription keyboard;
  keyboard.id = 0;
  keyboard.name = "Computer keyboard";
  keyboard.client = 0;
  keyboard.port = 0;
  result.push_back(keyboard);
  if (midi_client) {
    for (ItemCount i = 0; i < MIDIGetNumberOfSources(); ++i) {
      MIDIEndpointRef endpoint = MIDIGetSource(i);
      if (endpoint)
        result.push_back(Describe(endpoint, result.size()));
    }
  }
  return result;
}

void MidiCommIn::UpdateDeviceList() {}

MidiCommIn::MidiCommIn(unsigned int device_id) : m_should_reconnect(false), m_mac(0) {
  MidiCommDescriptionList devices = GetDeviceList();
  if (device_id >= devices.size())
    throw std::runtime_error("MIDI input device is no longer available");
  m_description = devices[device_id];
  m_mac = new MidiCommInMacState;
  m_mac->generation = device_generation.load();
  if (m_description.port == 0) {
    std::lock_guard<std::mutex> lock(keyboard_mutex);
    keyboard_input = m_mac;
  } else {
    OSStatus status = MIDIInputPortCreate(midi_client, CFSTR("Linthesia Input"), ReadPackets, m_mac, &m_mac->port);
    if (status == noErr) {
      m_mac->source = static_cast<MIDIEndpointRef>(m_description.port);
      status = MIDIPortConnectSource(m_mac->port, m_mac->source, 0);
    }
    if (status != noErr)
      std::fprintf(stderr, "Cannot connect MIDI input %s (%d).\n", m_description.name.c_str(), int(status));
  }
}

MidiCommIn::~MidiCommIn() {
  {
    std::lock_guard<std::mutex> lock(keyboard_mutex);
    if (keyboard_input == m_mac)
      keyboard_input = 0;
  }
  if (m_mac->port)
    MIDIPortDispose(m_mac->port);
  delete m_mac;
}

MidiEvent MidiCommIn::Read() {
  std::lock_guard<std::mutex> lock(m_mac->mutex);
  if (m_mac->events.empty())
    return MidiEvent::NullEvent();
  MidiEvent result = m_mac->events.front();
  m_mac->events.pop();
  return result;
}

bool MidiCommIn::KeepReading() const {
  std::lock_guard<std::mutex> lock(m_mac->mutex);
  return !m_mac->events.empty();
}

void MidiCommIn::Reset() {
  std::lock_guard<std::mutex> lock(m_mac->mutex);
  std::queue<MidiEvent> empty;
  m_mac->events.swap(empty);
  m_mac->status = 0;
  m_mac->length = 0;
}

bool MidiCommIn::ShouldReconnect() const {
  return m_description.port != 0 && m_mac->generation != device_generation.load();
}

void MidiCommIn::Reconnect() {
  m_mac->generation = device_generation.load();
  if (m_description.port == 0 || !m_mac->port)
    return;
  MIDIObjectRef object = 0;
  MIDIObjectType type;
  if (MIDIObjectFindByUniqueID(m_description.client, &object, &type) != noErr || type != kMIDIObjectType_Source)
    return;
  if (m_mac->source)
    MIDIPortDisconnectSource(m_mac->port, m_mac->source);
  m_mac->source = static_cast<MIDIEndpointRef>(object);
  m_description.port = static_cast<int>(object);
  MIDIPortConnectSource(m_mac->port, m_mac->source, 0);
}

MidiCommDescriptionList MidiCommOut::GetDeviceList() {
  midiInit();
  MidiCommDescriptionList result;
  if (synth_unit) {
    MidiCommDescription synth;
    synth.id = 0;
    synth.name = "macOS General MIDI (built-in sound)";
    synth.client = 0;
    synth.port = 0;
    result.push_back(synth);
  }
  if (midi_client) {
    for (ItemCount i = 0; i < MIDIGetNumberOfDestinations(); ++i) {
      MIDIEndpointRef endpoint = MIDIGetDestination(i);
      if (endpoint)
        result.push_back(Describe(endpoint, result.size()));
    }
  }
  return result;
}

void MidiCommOut::UpdateDeviceList() {}

MidiCommOut::MidiCommOut(unsigned int device_id) {
  MidiCommDescriptionList devices = GetDeviceList();
  if (device_id >= devices.size())
    throw std::runtime_error("MIDI output device is no longer available");
  m_description = devices[device_id];
}

MidiCommOut::~MidiCommOut() { Release(); }

void MidiCommOut::Release() { Reset(); }

void MidiCommOut::Write(const MidiEvent &event) {
  MidiEventSimple simple;
  if (event.GetSimpleEvent(&simple))
    OutputEvent(m_description, simple.status, simple.byte1, simple.byte2);
}

void MidiCommOut::Reset() {
  for (unsigned char channel = 0; channel < 16; ++channel) {
    OutputEvent(m_description, 0xB0 | channel, 64, 0);  // Sustain pedal off.
    OutputEvent(m_description, 0xB0 | channel, 123, 0); // All notes off.
    OutputEvent(m_description, 0xB0 | channel, 120, 0); // All sound off.
    OutputEvent(m_description, 0xB0 | channel, 121, 0); // Reset controllers.
  }
  notes_on.clear();
}

void MidiCommOut::Reconnect() {
  if (m_description.port == 0)
    return;
  MIDIObjectRef object = 0;
  MIDIObjectType type;
  if (MIDIObjectFindByUniqueID(m_description.client, &object, &type) == noErr && type == kMIDIObjectType_Destination)
    m_description.port = static_cast<int>(object);
}
