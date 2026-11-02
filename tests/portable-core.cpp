// Standalone regression checks for the portable MIDI parser and text helpers.
#include "libmidi/Midi.h"
#include "StringUtil.h"

#include <cstdint>
#include <cstring>
#include <iomanip>
#include <iostream>
#include <sstream>
#include <stdexcept>

static void Check(bool condition, const char* message) {
  if (!condition)
    throw std::runtime_error(message);
}

int main(int argc, char** argv) {
  try {
    const unsigned char big32[] = {0x12, 0x34, 0x56, 0x78};
    const unsigned char big16[] = {0x12, 0x34};
    uint32_t raw32;
    unsigned short raw16;
    std::memcpy(&raw32, big32, sizeof(raw32));
    std::memcpy(&raw16, big16, sizeof(raw16));
    Check(BigToSystem32(raw32) == 0x12345678, "32-bit MIDI endian conversion");
    Check(BigToSystem16(raw16) == 0x1234, "16-bit MIDI endian conversion");
    Check(STRING(std::fixed << std::setprecision(2) << 1.25) == "1.25",
          "String formatting manipulators");
    Check(StringLower(std::string("SONG.MIDI")) == "song.midi", "File extension casing");

    // Format 0, one middle C lasting a quarter note at 120 beats per minute.
    const unsigned char bytes[] = {
      'M','T','h','d', 0,0,0,6, 0,0, 0,1, 1,0xe0,
      'M','T','r','k', 0,0,0,20,
      0,0xff,0x51,3,7,0xa1,0x20,
      0,0x90,60,100,
      0x83,0x60,0x80,60,0,
      0,0xff,0x2f,0
    };
    std::istringstream stream(std::string(reinterpret_cast<const char*>(bytes), sizeof(bytes)));
    Midi midi = Midi::ReadFromStream(stream);
    Check(midi.AggregateNoteCount() == 1, "Single-note MIDI parsing");
    Check(midi.GetSongLengthInMicroseconds() >= 500000 &&
          midi.GetSongLengthInMicroseconds() <= 500001, "Tempo and tick conversion");

    std::istringstream truncated("MThd");
    bool rejected = false;
    try { Midi::ReadFromStream(truncated); }
    catch (const MidiError&) { rejected = true; }
    Check(rejected, "Truncated MIDI rejection");

    // Optionally check bundled/user MIDI files without requiring them for the test.
    for (int i = 1; i < argc; ++i)
      Check(Midi::ReadFromFile(argv[i]).AggregateNoteCount() > 0, "MIDI file reading");
    std::cout << "Portable core checks passed\n";
    return 0;
  } catch (const std::exception& error) {
    std::cerr << error.what() << '\n';
    return 1;
  }
}
