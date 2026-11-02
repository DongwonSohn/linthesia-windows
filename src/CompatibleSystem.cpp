// -*- mode: c++; coding: utf-8 -*-

// Linthesia

// Copyright (c) 2007 Nicholas Piegdon
// Adaptation to GNU/Linux by Oscar Aceña
// See COPYING for license information

#include <chrono>

#include "MidiComm.h"
#include "CompatibleSystem.h"
#include "StringUtil.h"
#include "Version.h"

#include <SDL.h>

using namespace std;

extern SDL_Window* sdl_window;
extern bool main_loop_running;

namespace Compatible {

  unsigned long GetMilliseconds() {

    return static_cast<unsigned long>(std::chrono::duration_cast<std::chrono::milliseconds>(
        std::chrono::steady_clock::now().time_since_epoch()).count());
  }

  unsigned long GetMicroseconds() {

    return static_cast<unsigned long>(std::chrono::duration_cast<std::chrono::microseconds>(
        std::chrono::steady_clock::now().time_since_epoch()).count());
  }


  void ShowError(const string &err) {

    const static string friendly_app_name =
      STRING("Linthesia " << LinthesiaVersionString);
    const static string message_box_title =
      STRING(friendly_app_name << " Error");

    printf("%s\n", err.c_str());
    SDL_ShowSimpleMessageBox(SDL_MESSAGEBOX_ERROR,
                               message_box_title.c_str(),
                               err.c_str(),
                               sdl_window);
  }

  void HideMouseCursor() {
    SDL_ShowCursor(SDL_DISABLE);
  }

  void ShowMouseCursor() {
    SDL_ShowCursor(SDL_ENABLE);
  }

  int GetWindowHeight() 
  {
    int h;
    SDL_GetWindowSize(sdl_window, nullptr, &h);
    return h;
  }


  void GracefulShutdown() {
    main_loop_running = false;
  }

}; // End namespace
