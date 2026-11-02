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

add_executable(Linthesia WIN32
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
  windows/Linthesia.rc)
target_include_directories(Linthesia PRIVATE src windows "${CMAKE_CURRENT_BINARY_DIR}")
target_compile_definitions(Linthesia PRIVATE
  NOMINMAX WIN32_LEAN_AND_MEAN _WIN32_WINNT=0x0A00
  GRAPHDIR="graphics" MUSICDIR="music")
if(MSVC)
  target_compile_options(Linthesia PRIVATE /utf-8 /W3)
else()
  target_compile_options(Linthesia PRIVATE -Wall -Wextra)
endif()
if(TARGET SDL2::SDL2main)
  target_link_libraries(Linthesia PRIVATE SDL2::SDL2main)
endif()
target_link_libraries(Linthesia PRIVATE
  SDL2::SDL2 SDL2_image::SDL2_image SDL2_ttf::SDL2_ttf
  SQLite3::SQLite3 OpenGL::GL Threads::Threads winmm shell32 ole32)

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
    "${CMAKE_CURRENT_SOURCE_DIR}/music/Keyboard_Classics/Fur_Elise-Beethoven.mid")
endif()

install(TARGETS Linthesia RUNTIME DESTINATION .)
install(DIRECTORY graphics/ DESTINATION graphics
  FILES_MATCHING PATTERN "*.tga" PATTERN "*.ttf" PATTERN "*.png")
install(DIRECTORY music/ DESTINATION music)
install(FILES COPYING DESTINATION .)
install(DIRECTORY windows/licenses/ DESTINATION licenses)
install(FILES WINDOWS-BUILD.ko.md DESTINATION . RENAME README-ko.md)
