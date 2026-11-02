// C ABI boundary between the standalone Windows launcher and the application.
#ifndef NOMINMAX
#define NOMINMAX
#endif
#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#include <windows.h>
#include <SDL.h>

extern "C" __declspec(dllexport) int WINAPI
LinthesiaRun(int argc, char** argv) noexcept {
  try {
    // SDL2main normally makes this call before invoking the application's main.
    SDL_SetMainReady();
    return SDL_main(argc, argv);
  } catch (...) {
    // Never let C++ exceptions or allocator ownership cross the DLL boundary.
    MessageBoxW(nullptr, L"Linthesia could not finish running. Restart the "
                         L"application and try again.",
                L"Linthesia", MB_OK | MB_ICONERROR);
    return 1;
  }
}
