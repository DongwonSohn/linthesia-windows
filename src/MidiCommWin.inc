// Native Windows MIDI input/output through WinMM. See COPYING for license.
// Included by MidiComm.cpp only on Windows; link with winmm.

#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#ifndef NOMINMAX
#define NOMINMAX
#endif
#include <windows.h>
#include <mmsystem.h>

#include "MidiComm.h"

#include <algorithm>
#include <chrono>
#include <cstdio>
#include <memory>
#include <mutex>
#include <stdexcept>

struct MidiCommInWinState {
  HMIDIIN handle = nullptr;
  CRITICAL_SECTION lock;
  static const unsigned int Capacity = 8192;
  DWORD messages[Capacity] = {};
  unsigned int head = 0, count = 0;
  bool accepting = true;
  bool reconnect = false;
  std::string devices;
  std::chrono::steady_clock::time_point last_check;

  MidiCommInWinState() { InitializeCriticalSection(&lock); }
  ~MidiCommInWinState() { DeleteCriticalSection(&lock); }
};

struct MidiCommOutWinState {
  HMIDIOUT handle = nullptr;
  bool failed = false;
  std::chrono::steady_clock::time_point last_retry;
};

namespace {
MidiCommInWinState *keyboard_input = nullptr;
std::mutex keyboard_lock;
// Device objects are owned and used by the application thread. Only the
// input queue is accessed by the WinMM callback thread.
std::vector<MidiCommInWinState *> inputs;
std::vector<MidiCommOutWinState *> outputs;

std::string Utf8(const wchar_t *value) {
  int length = WideCharToMultiByte(CP_UTF8, 0, value, -1, nullptr, 0, nullptr, nullptr);
  if (length <= 1)
    return std::string();
  std::string result(static_cast<size_t>(length), '\0');
  WideCharToMultiByte(CP_UTF8, 0, value, -1, &result[0], length, nullptr, nullptr);
  result.pop_back();
  return result;
}

std::string ErrorText(MMRESULT error, bool input) {
  wchar_t message[MAXERRORLENGTH] = {};
  MMRESULT result = input ? midiInGetErrorTextW(error, message, MAXERRORLENGTH)
                          : midiOutGetErrorTextW(error, message, MAXERRORLENGTH);
  return result == MMSYSERR_NOERROR ? Utf8(message) : std::to_string(error);
}

void QueueMessage(MidiCommInWinState *state, DWORD message) {
  EnterCriticalSection(&state->lock);
  if (state->accepting && state->count < MidiCommInWinState::Capacity) {
    state->messages[(state->head + state->count) % MidiCommInWinState::Capacity] = message;
    ++state->count;
  }
  LeaveCriticalSection(&state->lock);
}

void CALLBACK InputCallback(HMIDIIN, UINT message, DWORD_PTR context,
                            DWORD_PTR data, DWORD_PTR) {
  if (message != MIM_DATA && message != MIM_MOREDATA)
    return;
  // No allocation or multimedia API calls in this callback. System realtime
  // and SysEx messages are not keyboard events and need no input buffers.
  unsigned char status = static_cast<unsigned char>(data);
  if (status >= 0x80 && status < 0xF0)
    QueueMessage(reinterpret_cast<MidiCommInWinState *>(context), static_cast<DWORD>(data));
}

bool CloseInput(MidiCommInWinState *state) {
  EnterCriticalSection(&state->lock);
  state->accepting = false;
  LeaveCriticalSection(&state->lock);
  if (!state->handle)
    return true;
  // Never hold the queue lock while waiting for the driver/callback to stop.
  midiInStop(state->handle);
  midiInReset(state->handle);
  MMRESULT result = midiInClose(state->handle);
  if (result != MMSYSERR_NOERROR && result != MMSYSERR_INVALHANDLE) {
    std::fprintf(stderr, "Cannot close MIDI input: %s\n", ErrorText(result, true).c_str());
    return false;
  }
  state->handle = nullptr;
  return true;
}

MMRESULT OpenInput(MidiCommInWinState *state, UINT device) {
  EnterCriticalSection(&state->lock);
  state->accepting = true;
  LeaveCriticalSection(&state->lock);
  MMRESULT result = midiInOpen(&state->handle, device,
      reinterpret_cast<DWORD_PTR>(&InputCallback), reinterpret_cast<DWORD_PTR>(state),
      CALLBACK_FUNCTION | MIDI_IO_STATUS);
  if (result == MMSYSERR_NOERROR) {
    result = midiInStart(state->handle);
    if (result != MMSYSERR_NOERROR)
      CloseInput(state);
  }
  return result;
}

void CloseOutput(MidiCommOutWinState *state) {
  if (!state->handle)
    return;
  midiOutReset(state->handle);
  MMRESULT result = midiOutClose(state->handle);
  if (result != MMSYSERR_NOERROR && result != MMSYSERR_INVALHANDLE)
    std::fprintf(stderr, "Cannot close MIDI output: %s\n", ErrorText(result, false).c_str());
  state->handle = nullptr;
}

std::string InputDevices() {
  std::string result;
  for (UINT i = 0; i < midiInGetNumDevs(); ++i) {
    MIDIINCAPSW caps = {};
    if (midiInGetDevCapsW(i, &caps, sizeof(caps)) == MMSYSERR_NOERROR)
      result += std::to_string(i) + ":" + Utf8(caps.szPname) + "\n";
  }
  return result;
}

int FindInput(const MidiCommDescription &description) {
  int fallback = -1;
  for (UINT i = 0; i < midiInGetNumDevs(); ++i) {
    MIDIINCAPSW caps = {};
    if (midiInGetDevCapsW(i, &caps, sizeof(caps)) != MMSYSERR_NOERROR ||
        Utf8(caps.szPname) != description.name)
      continue;
    if (static_cast<int>(i) == description.client)
      return static_cast<int>(i);
    if (fallback < 0)
      fallback = static_cast<int>(i);
  }
  return fallback;
}

void SendOutput(MidiCommOutWinState *state, unsigned char status,
                unsigned char data1, unsigned char data2) {
  if (!state->handle || state->failed)
    return;
  DWORD message = status | (static_cast<DWORD>(data1 & 0x7F) << 8) |
                  (static_cast<DWORD>(data2 & 0x7F) << 16);
  MMRESULT result = midiOutShortMsg(state->handle, message);
  if (result != MMSYSERR_NOERROR) {
    state->failed = true; // Report once instead of once per note/frame.
    std::fprintf(stderr, "MIDI output unavailable: %s\n", ErrorText(result, false).c_str());
  }
}
}

void midiInit() {}

void midiStop() {
  {
    std::lock_guard<std::mutex> lock(keyboard_lock);
    keyboard_input = nullptr;
  }
  for (auto *state : inputs)
    CloseInput(state);
  for (auto *state : outputs)
    CloseOutput(state);
}

void sendNote(const unsigned char note, bool on) {
  if (note > 127)
    return;
  std::lock_guard<std::mutex> lock(keyboard_lock);
  if (keyboard_input)
    QueueMessage(keyboard_input, (on ? 0x90u : 0x80u) |
        (static_cast<DWORD>(note) << 8) | (on ? 60u << 16 : 0u));
}

MidiCommDescriptionList MidiCommIn::GetDeviceList() {
  MidiCommDescriptionList result;
  result.push_back({0, "Computer keyboard", -1, 0});
  for (UINT i = 0; i < midiInGetNumDevs(); ++i) {
    MIDIINCAPSW caps = {};
    if (midiInGetDevCapsW(i, &caps, sizeof(caps)) == MMSYSERR_NOERROR)
      result.push_back({static_cast<unsigned int>(result.size()), Utf8(caps.szPname),
                        static_cast<int>(i), 1});
  }
  return result;
}

void MidiCommIn::UpdateDeviceList() {}

MidiCommIn::MidiCommIn(unsigned int device_id) : m_should_reconnect(false), m_win(nullptr) {
  MidiCommDescriptionList devices = GetDeviceList();
  if (device_id >= devices.size())
    throw std::runtime_error("MIDI input device is no longer available.");
  m_description = devices[device_id];
  std::unique_ptr<MidiCommInWinState> state(new MidiCommInWinState);
  state->devices = InputDevices();
  state->last_check = std::chrono::steady_clock::now();
  if (m_description.port != 0) {
    MMRESULT result = OpenInput(state.get(), static_cast<UINT>(m_description.client));
    if (result != MMSYSERR_NOERROR) {
      if (!CloseInput(state.get()))
        state.release(); // Preserve a driver callback context if close fails.
      throw std::runtime_error("Cannot open MIDI input '" + m_description.name + "': " +
                               ErrorText(result, true));
    }
  }
  inputs.push_back(state.get());
  m_win = state.release();
  if (m_description.port == 0) {
    std::lock_guard<std::mutex> lock(keyboard_lock);
    keyboard_input = m_win;
  }
}

MidiCommIn::~MidiCommIn() {
  {
    std::lock_guard<std::mutex> lock(keyboard_lock);
    if (keyboard_input == m_win)
      keyboard_input = nullptr;
  }
  inputs.erase(std::remove(inputs.begin(), inputs.end(), m_win), inputs.end());
  // If a broken driver refuses to close, retain its callback context rather
  // than freeing memory it could still access. Normally no buffers are pending.
  if (CloseInput(m_win))
    delete m_win;
}

MidiEvent MidiCommIn::Read() {
  EnterCriticalSection(&m_win->lock);
  if (!m_win->count) {
    LeaveCriticalSection(&m_win->lock);
    return MidiEvent::NullEvent();
  }
  DWORD message = m_win->messages[m_win->head];
  m_win->head = (m_win->head + 1) % MidiCommInWinState::Capacity;
  --m_win->count;
  LeaveCriticalSection(&m_win->lock);
  MidiEventSimple event(static_cast<unsigned char>(message),
      static_cast<unsigned char>((message >> 8) & 0x7F),
      static_cast<unsigned char>((message >> 16) & 0x7F));
  if ((event.status & 0xF0) == 0x90 && event.byte2 == 0)
    event.status = 0x80 | (event.status & 0x0F);
  return MidiEvent::Build(event);
}

bool MidiCommIn::KeepReading() const {
  if (m_description.port != 0) {
    auto now = std::chrono::steady_clock::now();
    if (now - m_win->last_check >= std::chrono::seconds(1)) {
      m_win->last_check = now;
      m_win->reconnect = m_win->devices != InputDevices();
    }
  }
  // Device changes must be delivered even when a disconnected keyboard no
  // longer produces notes; PlayingState checks reconnect inside its read loop.
  if (m_win->reconnect)
    return true;
  EnterCriticalSection(&m_win->lock);
  bool result = m_win->count != 0;
  LeaveCriticalSection(&m_win->lock);
  return result;
}

void MidiCommIn::Reset() {
  EnterCriticalSection(&m_win->lock);
  m_win->head = m_win->count = 0;
  LeaveCriticalSection(&m_win->lock);
}

bool MidiCommIn::ShouldReconnect() const { return m_win->reconnect; }

void MidiCommIn::Reconnect() {
  m_win->reconnect = false;
  m_win->devices = InputDevices();
  if (m_description.port == 0 || !CloseInput(m_win))
    return;
  Reset();
  int device = FindInput(m_description);
  if (device < 0)
    return;
  m_description.client = device;
  MMRESULT result = OpenInput(m_win, static_cast<UINT>(device));
  if (result != MMSYSERR_NOERROR)
    std::fprintf(stderr, "Cannot reconnect MIDI input: %s\n", ErrorText(result, true).c_str());
}

MidiCommDescriptionList MidiCommOut::GetDeviceList() {
  MidiCommDescriptionList result;
  if (midiOutGetNumDevs() > 0)
    result.push_back({0, "Windows default MIDI output (MIDI Mapper)", -1, -1});
  for (UINT i = 0; i < midiOutGetNumDevs(); ++i) {
    MIDIOUTCAPSW caps = {};
    if (midiOutGetDevCapsW(i, &caps, sizeof(caps)) == MMSYSERR_NOERROR)
      result.push_back({static_cast<unsigned int>(result.size()), Utf8(caps.szPname),
                        static_cast<int>(i), static_cast<int>(i)});
  }
  return result;
}

void MidiCommOut::UpdateDeviceList() {}

MidiCommOut::MidiCommOut(unsigned int device_id) : m_win(nullptr) {
  MidiCommDescriptionList devices = GetDeviceList();
  if (device_id >= devices.size())
    throw std::runtime_error("MIDI output device is no longer available.");
  m_description = devices[device_id];
  std::unique_ptr<MidiCommOutWinState> state(new MidiCommOutWinState);
  MMRESULT result = midiOutOpen(&state->handle, static_cast<UINT>(m_description.client),
                                0, 0, CALLBACK_NULL);
  if (result != MMSYSERR_NOERROR)
    throw std::runtime_error("Cannot open MIDI output '" + m_description.name + "': " +
                             ErrorText(result, false));
  outputs.push_back(state.get());
  m_win = state.release();
}

MidiCommOut::~MidiCommOut() {
  Release();
  outputs.erase(std::remove(outputs.begin(), outputs.end(), m_win), outputs.end());
  delete m_win;
}

void MidiCommOut::Release() {
  Reset();
  CloseOutput(m_win);
}

void MidiCommOut::Write(const MidiEvent &event) {
  if (!m_win->handle || m_win->failed) {
    auto now = std::chrono::steady_clock::now();
    if (now - m_win->last_retry >= std::chrono::seconds(1)) {
      m_win->last_retry = now;
      Reconnect();
    }
  }
  MidiEventSimple simple;
  if (event.GetSimpleEvent(&simple) && simple.status >= 0x80 && simple.status < 0xF0)
    SendOutput(m_win, simple.status, simple.byte1, simple.byte2);
}

void MidiCommOut::Reset() {
  for (unsigned char channel = 0; channel < 16; ++channel) {
    SendOutput(m_win, 0xB0 | channel, 64, 0);  // Sustain off.
    SendOutput(m_win, 0xB0 | channel, 123, 0); // All notes off.
    SendOutput(m_win, 0xB0 | channel, 120, 0); // All sound off.
    SendOutput(m_win, 0xB0 | channel, 121, 0); // Reset controllers.
  }
  if (m_win->handle)
    midiOutReset(m_win->handle);
  notes_on.clear();
}

void MidiCommOut::Reconnect() {
  if (m_win->handle && !m_win->failed)
    return;
  CloseOutput(m_win);
  int device = -1;
  if (m_description.client >= 0) {
    device = -2;
    for (UINT i = 0; i < midiOutGetNumDevs(); ++i) {
      MIDIOUTCAPSW caps = {};
      if (midiOutGetDevCapsW(i, &caps, sizeof(caps)) == MMSYSERR_NOERROR &&
          Utf8(caps.szPname) == m_description.name) {
        device = static_cast<int>(i);
        break;
      }
    }
  }
  if (device == -2)
    return;
  MMRESULT result = midiOutOpen(&m_win->handle, static_cast<UINT>(device), 0, 0, CALLBACK_NULL);
  m_win->failed = result != MMSYSERR_NOERROR;
  if (m_win->failed)
    std::fprintf(stderr, "Cannot reconnect MIDI output: %s\n", ErrorText(result, false).c_str());
  else
    m_description.client = device;
}
