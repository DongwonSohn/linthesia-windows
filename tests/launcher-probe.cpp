// Test-only application DLL: exercise the launcher's exported C boundary with
// the existing MIDI parser checks, without opening a window or MIDI device.
#ifndef NOMINMAX
#define NOMINMAX
#endif
#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#include <windows.h>

int LinthesiaPortableCoreMain(int argc, char** argv);

extern "C" __declspec(dllexport) int WINAPI LinthesiaRun(int argc, char** argv) noexcept {
  // The package test supplies two relative Unicode filenames. Requiring both
  // prevents a launcher that drops every argument from reporting a false pass.
  if (argc != 3 || !argv || !argv[0] || !argv[1] || !argv[2])
    return 97;
  try {
    return LinthesiaPortableCoreMain(argc, argv);
  } catch (...) {
    return 1;
  }
}
