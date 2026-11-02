// -*- mode: c++; coding: utf-8 -*-

// Linthesia

// Copyright (c) 2007 Nicholas Piegdon
// Adaptation to GNU/Linux by Oscar Aceña
// See COPYING for license information

#include "UserSettings.h"
#include "StringUtil.h"
#include "PlatformPaths.h"
#if defined(__APPLE__) || defined(_WIN32)
#include <cstdlib>
#include <cstdio>
#include <fstream>
#include <iomanip>
#include <map>
#include <stdexcept>
#include <sys/stat.h>
#include <errno.h>
#ifdef _WIN32
#include <SDL.h>
#endif
#else
#include <giomm/settings.h>
#endif

using namespace std;

namespace UserSetting {

  static bool g_initialized(false);
#if defined(__APPLE__) || defined(_WIN32)
  static map<string, string> settings;
  static string data_directory;

  string DataDirectory() {
    if (!data_directory.empty())
      return data_directory;
#ifdef _WIN32
    char* path = SDL_GetPrefPath("", "Linthesia");
    if (!path)
      throw runtime_error(string("Cannot create Linthesia settings directory: ") + SDL_GetError());
    data_directory = path;
    SDL_free(path);
    while (!data_directory.empty() && (data_directory.back() == '/' || data_directory.back() == '\\'))
      data_directory.pop_back();
#else
    const char* home = getenv("HOME");
    if (!home || !*home)
      throw runtime_error("Cannot find the home directory for Linthesia settings.");
    data_directory = string(home) + "/Library/Application Support/Linthesia";
    if (mkdir(data_directory.c_str(), 0700) != 0 && errno != EEXIST)
      throw runtime_error("Cannot create Linthesia settings directory: " + data_directory);
#endif
    return data_directory;
  }

  void Initialize() {
    if (g_initialized)
      return;
    ifstream input(LinthesiaNativePath(DataDirectory() + "/settings.txt").c_str());
    string key, value;
    while (input >> quoted(key) >> quoted(value))
      settings[key] = value;
    g_initialized = true;
  }

  string Get(const string& setting, const string& default_value) {
    auto it = settings.find(setting);
    return it == settings.end() || it->second.empty() ? default_value : it->second;
  }

  void Set(const string& setting, const string& value) {
    if (!g_initialized)
      return;
    settings[setting] = value;
    const string filename = DataDirectory() + "/settings.txt";
    const string temporary = filename + ".tmp";
    {
      ofstream output(LinthesiaNativePath(temporary).c_str(), ios::trunc);
      if (!output)
        throw runtime_error("Cannot save Linthesia settings.");
      for (const auto& entry : settings)
        output << quoted(entry.first) << ' ' << quoted(entry.second) << '\n';
      output.close();
      if (!output)
        throw runtime_error("Cannot finish saving Linthesia settings.");
    }
#ifdef _WIN32
    if (!MoveFileExW(LinthesiaNativePath(temporary).c_str(), LinthesiaNativePath(filename).c_str(),
                     MOVEFILE_REPLACE_EXISTING | MOVEFILE_WRITE_THROUGH))
#else
    if (rename(temporary.c_str(), filename.c_str()) != 0)
#endif
      throw runtime_error("Cannot replace Linthesia settings file.");
  }
#else
  Glib::RefPtr<Gio::Settings> settings;

  void Initialize() {
    if (g_initialized) 
      return;

    settings = Gio::Settings::create(SCHEMA, SCHEMA_PATH);
    g_initialized = true;
  }

  string Get(const string &setting, const string &default_value) {
    if (!g_initialized) 
      return default_value;

    string value = settings->get_string(Glib::ustring(setting)).raw();

    if (value == "")
      return default_value;
    
    return value;
  }
    
  void Set(const string &setting, const string &value) {
    if (!g_initialized) 
      return;

    settings->set_string(setting.c_str(), value.c_str());
  }
#endif

}; // End namespace
