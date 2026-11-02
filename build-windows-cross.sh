#!/usr/bin/env bash
# Cross-build Windows x64 from macOS/Linux using the official Zig compiler.
# Set ZIG to the Zig 0.17.0 executable and LINTHESIA_WINDOWS_DEPS to a directory
# containing these official archives, extracted without renaming their folders:
# https://www.libsdl.org/release/SDL2-devel-2.32.10-mingw.tar.gz
# https://www.libsdl.org/projects/SDL_ttf/release/SDL2_ttf-devel-2.24.0-mingw.tar.gz
# https://www.libsdl.org/projects/SDL_image/release/SDL2_image-devel-2.8.12-mingw.tar.gz
# https://www.sqlite.org/2026/sqlite-amalgamation-3540100.zip
# Zig: https://ziglang.org/download/0.17.0/
set -euo pipefail
src="$(cd "$(dirname "$0")" && pwd)"
: "${ZIG:?Set ZIG to the Zig 0.17.0 executable}"
: "${LINTHESIA_WINDOWS_DEPS:?Set LINTHESIA_WINDOWS_DEPS to the extracted Windows dependency directory}"
deps="$LINTHESIA_WINDOWS_DEPS"
sdl="$deps/SDL2-2.32.10/x86_64-w64-mingw32"
ttf="$deps/SDL2_ttf-2.24.0/x86_64-w64-mingw32"
img="$deps/SDL2_image-2.8.12/x86_64-w64-mingw32"
sqlite="$deps/sqlite-amalgamation-3540100"
build="$src/build-windows-cross"
for input in "$sdl/lib/libSDL2.dll.a" "$ttf/lib/libSDL2_ttf.dll.a" "$img/lib/libSDL2_image.dll.a" "$sqlite/sqlite3.c"; do
  [[ -f "$input" ]] || { printf 'Missing dependency: %s\n' "$input" >&2; exit 1; }
done
mkdir -p "$build"
printf '#define PACKAGE_VERSION "0.8.0-windows"\n' > "$build/config.h"
"$ZIG" cc -target x86_64-windows-gnu -O2 -c "$sqlite/sqlite3.c" -o "$build/sqlite3.o"
"$ZIG" rc /fo "$build/Linthesia.res" /i "$src/windows" -- "$src/windows/Linthesia.rc"
"$ZIG" c++ -target x86_64-windows-gnu -std=c++14 -O2 \
  -DNOMINMAX -DWIN32_LEAN_AND_MEAN -D_WIN32_WINNT=0x0a00 \
  -Wno-inconsistent-missing-override \
  -I "$sdl/include/SDL2" -I "$ttf/include/SDL2" -I "$img/include/SDL2" \
  -I "$sqlite" -I "$build" -I "$src/src" \
  "$src"/src/*.cpp "$src"/src/libmidi/*.cpp "$build/sqlite3.o" "$build/Linthesia.res" \
  "$sdl/lib/libSDL2main.a" "$sdl/lib/libSDL2.dll.a" \
  "$ttf/lib/libSDL2_ttf.dll.a" "$img/lib/libSDL2_image.dll.a" \
  -lopengl32 -lwinmm -lshell32 -lole32 -Wl,--subsystem,windows -o "$build/Linthesia.exe"
"$ZIG" c++ -target x86_64-windows-gnu -std=c++14 -O2 -static \
  -DNOMINMAX -DWIN32_LEAN_AND_MEAN -D_WIN32_WINNT=0x0a00 \
  "$src/windows/Launcher.cpp" "$build/Linthesia.res" \
  -luser32 -lshell32 -Wl,--subsystem,windows -o "$build/LinthesiaLauncher.exe"

# Generate the same portable directory layout as the native Windows build.
python3 - "$src" "$deps" "$build" <<'PY'
import pathlib, shutil, sys, zipfile
src, deps, build = map(pathlib.Path, sys.argv[1:])
dist = src / 'dist' / 'windows-x64'
stage = dist / 'Linthesia'
if stage.exists():
    shutil.rmtree(stage)
app = stage / 'app'
(app / 'graphics').mkdir(parents=True)
(app / 'licenses').mkdir()
shutil.copy2(build / 'LinthesiaLauncher.exe', stage / 'Linthesia.exe')
shutil.copy2(build / 'Linthesia.exe', app)
for path in (src / 'graphics').iterdir():
    if path.suffix.lower() in ('.tga', '.ttf', '.png'):
        shutil.copy2(path, app / 'graphics')
shutil.copytree(src / 'music', app / 'music')
shutil.copy2(src / 'COPYING', app)
shutil.copy2(src / 'WINDOWS-BUILD.ko.md', app / 'README-ko.md')
shutil.copytree(src / 'windows' / 'licenses', app / 'licenses', dirs_exist_ok=True)
for name, version in [('SDL2', '2.32.10'), ('SDL2_ttf', '2.24.0'), ('SDL2_image', '2.8.12')]:
    library = deps / (name + '-' + version)
    shutil.copy2(library / 'x86_64-w64-mingw32' / 'bin' / (name + '.dll'), app)
    shutil.copy2(library / 'LICENSE.txt', app / 'licenses' / (name + '-LICENSE.txt'))
    readme = library / 'README.txt'
    if readme.exists():
        shutil.copy2(readme, app / 'licenses' / (name + '-README.txt'))
(app / 'licenses' / 'SQLite.txt').write_text(
    'SQLite 3.54.1 is in the public domain.\nhttps://www.sqlite.org/copyright.html\n', encoding='utf-8')
archive = dist / 'Linthesia-windows-x64.zip'
with zipfile.ZipFile(archive, 'w', compression=zipfile.ZIP_DEFLATED) as z:
    for path in sorted(stage.rglob('*')):
        if path.is_file():
            z.write(path, path.relative_to(stage))
print('Executable:', stage / 'Linthesia.exe')
print('Portable ZIP:', archive)
PY
