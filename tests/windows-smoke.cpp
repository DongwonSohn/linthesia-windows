// Windows integration checks that require no MIDI hardware or user settings.
#include "PlatformPaths.h"
#include "MidiComm.h"
#include "libmidi/Midi.h"
#include <sqlite3.h>

#include <fstream>
#include <iostream>
#include <stdexcept>

namespace {
void Require(bool condition, const char *message) {
  if (!condition)
    throw std::runtime_error(message);
}

struct TemporaryFiles {
  std::wstring root, directory, midi, database;

  TemporaryFiles() {
    wchar_t path[32768] = {};
    DWORD length = GetTempPathW(32768, path);
    Require(length > 0 && length < 32768, "Cannot locate Windows temporary directory.");
    wchar_t unique[MAX_PATH] = {};
    Require(GetTempFileNameW(path, L"lth", 0, unique) != 0, "Cannot reserve temporary path.");
    root = unique;
    Require(DeleteFileW(root.c_str()) != 0, "Cannot remove temporary placeholder.");
    Require(CreateDirectoryW(root.c_str(), nullptr) != 0, "Cannot create temporary directory.");
    // Korean, Greek, and spaces catch accidental conversion through an ANSI code page.
    directory = root + L"/\uD53C\uC544\uB178 \u03A9";
    if (!CreateDirectoryW(directory.c_str(), nullptr)) {
      RemoveDirectoryW(root.c_str());
      throw std::runtime_error("Cannot create Unicode temporary directory.");
    }
    midi = directory + L"/\uC5F0\uC8FC \uACE1.mid";
    database = directory + L"/\uC74C\uC545 \uBAA9\uB85D.sqlite";
  }

  ~TemporaryFiles() {
    DeleteFileW(midi.c_str());
    DeleteFileW(database.c_str());
    RemoveDirectoryW(directory.c_str());
    RemoveDirectoryW(root.c_str());
  }
};

void CheckKeyboard() {
  MidiCommDescriptionList devices = MidiCommIn::GetDeviceList();
  Require(!devices.empty() && devices[0].id == 0 && devices[0].port == 0,
          "Computer keyboard must always be available as input zero.");
  MidiCommIn keyboard(0);
  Require(!keyboard.KeepReading(), "New keyboard input should have no queued events.");
  sendNote(60, true);
  sendNote(60, false);
  Require(keyboard.KeepReading(), "Computer keyboard note-on was not queued.");
  MidiEvent event = keyboard.Read();
  Require(event.Type() == MidiEventType_NoteOn && event.NoteNumber() == 60 &&
          event.NoteVelocity() == 60, "Computer keyboard note-on data was changed.");
  Require(keyboard.KeepReading(), "Computer keyboard note-off was not queued.");
  event = keyboard.Read();
  Require(event.Type() == MidiEventType_NoteOff && event.NoteNumber() == 60,
          "Computer keyboard note-off data was changed.");
  Require(!keyboard.KeepReading(), "Input queue did not drain.");
  sendNote(64, true);
  keyboard.Reset();
  Require(!keyboard.KeepReading(), "Reset must discard buffered keyboard events.");
  sendNote(128, true);
  Require(!keyboard.KeepReading(), "Out-of-range notes must be ignored.");
}

void CheckUnicodeFiles() {
  TemporaryFiles files;
  const std::string midi_path = LinthesiaUtf8(files.midi);
  const std::string database_path = LinthesiaUtf8(files.database);
  Require(LinthesiaNativePath(midi_path) == files.midi, "Unicode MIDI path roundtrip failed.");
  Require(LinthesiaNativePath(database_path) == files.database,
          "Unicode database path roundtrip failed.");
  Require(LinthesiaIsDirectory(LinthesiaUtf8(files.directory)),
          "Unicode directory lookup failed.");
  bool rejected_invalid_utf8 = false;
  try {
    LinthesiaNativePath(std::string(1, static_cast<char>(0xFF)));
  } catch (const std::runtime_error &) {
    rejected_invalid_utf8 = true;
  }
  Require(rejected_invalid_utf8, "Invalid UTF-8 paths should be rejected.");

  // A complete format-zero MIDI file with one middle-C note and an end marker.
  const unsigned char fixture[] = {
    'M', 'T', 'h', 'd', 0, 0, 0, 6, 0, 0, 0, 1, 0, 96,
    'M', 'T', 'r', 'k', 0, 0, 0, 12,
    0, 0x90, 60, 64, 96, 0x80, 60, 0, 0, 0xFF, 0x2F, 0
  };
  {
    std::ofstream file(LinthesiaNativePath(midi_path).c_str(), std::ios::binary);
    Require(file.is_open(), "Cannot create MIDI file at Unicode path.");
    file.write(reinterpret_cast<const char *>(fixture), sizeof(fixture));
    file.close();
    Require(!file.fail(), "Cannot finish writing Unicode MIDI fixture.");
  }
  Require(LinthesiaIsFile(midi_path), "Unicode MIDI file lookup failed.");
  Midi song = Midi::ReadFromFile(midi_path);
  Require(song.AggregateNoteCount() == 1 && song.Notes().size() == 1,
          "MIDI fixture at Unicode path was not parsed correctly.");
  const TranslatedNote &note = *song.Notes().begin();
  Require(note.note_id == 60 && note.channel == 0 && note.velocity == 64 &&
          note.end - note.start == 500000,
          "MIDI fixture note, channel, velocity, or duration was changed.");

  sqlite3 *database = nullptr;
  int result = sqlite3_open(database_path.c_str(), &database);
  if (result != SQLITE_OK) {
    if (database)
      sqlite3_close(database);
    throw std::runtime_error("Cannot open SQLite database at Unicode path.");
  }
  result = sqlite3_exec(database,
      "CREATE TABLE smoke (value INTEGER); INSERT INTO smoke VALUES (42);",
      nullptr, nullptr, nullptr);
  if (result != SQLITE_OK) {
    sqlite3_close(database);
    throw std::runtime_error("Cannot write SQLite database at Unicode path.");
  }
  sqlite3_stmt *statement = nullptr;
  result = sqlite3_prepare_v2(database, "SELECT value FROM smoke", -1, &statement, nullptr);
  bool read_ok = result == SQLITE_OK && sqlite3_step(statement) == SQLITE_ROW &&
                 sqlite3_column_int(statement, 0) == 42;
  sqlite3_finalize(statement);
  int close_result = sqlite3_close(database);
  Require(read_ok && close_result == SQLITE_OK, "Unicode SQLite database check failed.");
  Require(LinthesiaIsFile(database_path), "Unicode SQLite file lookup failed.");
}
}

int main() {
  try {
    midiInit();
    CheckKeyboard();
    CheckUnicodeFiles();
    midiStop();
    // Sending a computer-keyboard event after its input object is destroyed
    // must be safe and must not access a stale callback context.
    sendNote(60, false);
    std::cout << "Windows MIDI keyboard and Unicode file/database checks passed.\n";
    return 0;
  } catch (const std::exception &error) {
    midiStop();
    std::cerr << error.what() << '\n';
  } catch (...) {
    midiStop();
    std::cerr << "Unexpected error in Windows smoke checks.\n";
  }
  return 1;
}
