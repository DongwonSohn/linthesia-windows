cmake_minimum_required(VERSION 3.21)
project(Linthesia VERSION 0.8.0 LANGUAGES CXX)

# The original Meson build remains available for Linux. This build produces a
# native Windows application with the MinGW-w64/UCRT64 toolchain.
if(NOT WIN32)
  message(FATAL_ERROR "This CMake build targets Windows. Use Meson on Linux or build-macos-local.sh on macOS.")
endif()

set(CMAKE_CXX_STANDARD 14)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)
enable_language(RC)

find_package(SDL2 CONFIG REQUIRED)
find_package(SDL2_image CONFIG REQUIRED)
find_package(SDL2_ttf CONFIG REQUIRED)
find_package(SQLite3 REQUIRED)
find_package(OpenGL REQUIRED)
find_package(Threads REQUIRED)
if(NOT TARGET SQLite3::SQLite3)
  add_library(SQLite3::SQLite3 ALIAS SQLite::SQLite3)
endif()

file(WRITE "${CMAKE_CURRENT_BINARY_DIR}/config.h"
  "#pragma once\n#define PACKAGE_VERSION \"${PROJECT_VERSION}-windows\"\n")

add_library(Linthesia SHARED
  src/MenuLayout.cpp src/CompatibleSystem.cpp src/StringTile.cpp
  src/TitleState.cpp src/Tga.cpp src/SongTile.cpp src/TrackTile.cpp
  src/UserSettings.cpp src/TextWriter.cpp src/EnumTile.cpp
  src/PlayingState.cpp src/KeyboardDisplay.cpp src/TrackSelectionState.cpp
  src/GameState.cpp src/Keyboard.cpp src/MidiComm.cpp src/Renderer.cpp
  src/DeviceTile.cpp src/FrameAverage.cpp src/StatsState.cpp
  src/LinthesiaError.cpp src/main.cpp src/SongLibState.cpp
  src/DpmsThread.cpp src/FileSelector.cpp
  src/libmidi/MidiEvent.cpp src/libmidi/MidiTrack.cpp
  src/libmidi/Midi.cpp src/libmidi/MidiUtil.cpp
  windows/AppEntry.cpp)
set_target_properties(Linthesia PROPERTIES PREFIX "" OUTPUT_NAME LinthesiaApp)
target_include_directories(Linthesia PRIVATE src windows "${CMAKE_CURRENT_BINARY_DIR}")
target_compile_definitions(Linthesia PRIVATE
  NOMINMAX WIN32_LEAN_AND_MEAN _WIN32_WINNT=0x0A00
  GRAPHDIR="graphics" MUSICDIR="music")
if(MSVC)
  target_compile_options(Linthesia PRIVATE /utf-8 /W3)
else()
  target_compile_options(Linthesia PRIVATE -Wall -Wextra)
endif()
target_link_libraries(Linthesia PRIVATE
  SDL2::SDL2 SDL2_image::SDL2_image SDL2_ttf::SDL2_ttf
  SQLite3::SQLite3 OpenGL::GL Threads::Threads winmm shell32 ole32)

# Load the application and runtime DLLs from dll/, keeping resources at the root.
add_executable(LinthesiaLauncher WIN32 windows/Launcher.cpp windows/Linthesia.rc)
target_include_directories(LinthesiaLauncher PRIVATE windows)
target_compile_definitions(LinthesiaLauncher PRIVATE
  NOMINMAX WIN32_LEAN_AND_MEAN _WIN32_WINNT=0x0A00)
target_link_libraries(LinthesiaLauncher PRIVATE user32 shell32)
set_target_properties(LinthesiaLauncher PROPERTIES
  OUTPUT_NAME Linthesia
  RUNTIME_OUTPUT_DIRECTORY "${CMAKE_CURRENT_BINARY_DIR}/launcher")
if(MINGW)
  target_link_options(LinthesiaLauncher PRIVATE -static -static-libgcc -static-libstdc++)
elseif(MSVC)
  set_property(TARGET LinthesiaLauncher PROPERTY
    MSVC_RUNTIME_LIBRARY "MultiThreaded$<$<CONFIG:Debug>:Debug>")
endif()

include(CTest)
if(BUILD_TESTING)
  add_executable(WindowsSmoke
    tests/windows-smoke.cpp src/MidiComm.cpp
    src/libmidi/MidiEvent.cpp src/libmidi/MidiTrack.cpp
    src/libmidi/Midi.cpp src/libmidi/MidiUtil.cpp)
  target_include_directories(WindowsSmoke PRIVATE src "${CMAKE_CURRENT_BINARY_DIR}")
  target_compile_definitions(WindowsSmoke PRIVATE
    NOMINMAX WIN32_LEAN_AND_MEAN _WIN32_WINNT=0x0A00)
  target_link_libraries(WindowsSmoke PRIVATE
    SQLite3::SQLite3 Threads::Threads winmm)
  add_test(NAME windows-smoke COMMAND WindowsSmoke)

  add_executable(PortableCore
    tests/portable-core.cpp windows/Linthesia.rc
    src/libmidi/MidiEvent.cpp src/libmidi/MidiTrack.cpp
    src/libmidi/Midi.cpp src/libmidi/MidiUtil.cpp)
  target_include_directories(PortableCore PRIVATE src windows)
  target_compile_definitions(PortableCore PRIVATE NOMINMAX WIN32_LEAN_AND_MEAN)
  add_test(NAME portable-core COMMAND PortableCore
    "${CMAKE_CURRENT_SOURCE_DIR}/music/Keyboard_Classics/Fur_Elise-Beethoven.mid"
    "${CMAKE_CURRENT_SOURCE_DIR}/music/Default/Eve ／ 夜は仄か.mid")

  # Exercise the launcher's UTF-8 argv, working directory, and return status
  # without requiring an OpenGL display or a physical MIDI device.
  add_library(LauncherProbe SHARED
    tests/launcher-probe.cpp tests/portable-core.cpp
    src/libmidi/MidiEvent.cpp src/libmidi/MidiTrack.cpp
    src/libmidi/Midi.cpp src/libmidi/MidiUtil.cpp)
  target_include_directories(LauncherProbe PRIVATE src)
  target_compile_definitions(LauncherProbe PRIVATE
    NOMINMAX WIN32_LEAN_AND_MEAN main=LinthesiaPortableCoreMain)
  set_target_properties(LauncherProbe PROPERTIES PREFIX "" OUTPUT_NAME LauncherProbe)
endif()

install(TARGETS LinthesiaLauncher RUNTIME DESTINATION .)
install(TARGETS Linthesia RUNTIME DESTINATION dll LIBRARY DESTINATION dll)
install(DIRECTORY graphics/ DESTINATION graphics
  FILES_MATCHING PATTERN "*.tga" PATTERN "*.ttf" PATTERN "*.png")
install(DIRECTORY music/ DESTINATION music)
install(FILES COPYING DESTINATION manual)
install(DIRECTORY windows/licenses/ DESTINATION licenses)
install(FILES WINDOWS-BUILD.ko.md DESTINATION manual RENAME README-ko.md)
