#!/bin/bash
set -euo pipefail
src="$(cd "$(dirname "$0")" && pwd)"
: "${ZIG:?Set ZIG to the Zig 0.17.0 executable}"
: "${SDKROOT:?Set SDKROOT to a locally extracted macOS SDK}"
: "${LINTHESIA_FRAMEWORKS:?Set LINTHESIA_FRAMEWORKS to the directory with the three SDL2 frameworks}"
mkdir -p "$src/build-macos"
export ZIG_GLOBAL_CACHE_DIR="$src/build-macos/zig-global-cache"
export ZIG_LOCAL_CACHE_DIR="$src/build-macos/zig-local-cache"
if [[ ! -e "$SDKROOT/LocalFrameworks" ]]; then
  ln -s "$LINTHESIA_FRAMEWORKS" "$SDKROOT/LocalFrameworks"
fi
printf '#define PACKAGE_VERSION "0.8.0-macos-local"\n' > "$src/build-macos/config.h"
"$ZIG" c++ -target aarch64-macos.13.0 -std=c++14 -O2 \
  --sysroot "$SDKROOT" -isystem "$SDKROOT/usr/include" -L /usr/lib \
  -F /System/Library/Frameworks -F /LocalFrameworks \
  -I "$LINTHESIA_FRAMEWORKS/SDL2.framework/Headers" \
  -I "$LINTHESIA_FRAMEWORKS/SDL2_ttf.framework/Headers" \
  -I "$LINTHESIA_FRAMEWORKS/SDL2_image.framework/Headers" \
  -I "$src/build-macos" -I "$src/src" \
  -Wno-deprecated-declarations -Wno-macro-redefined -Wno-elaborated-enum-base \
  -Wno-inconsistent-missing-override \
  "$src"/src/*.cpp "$src"/src/libmidi/*.cpp \
  -framework SDL2 -framework SDL2_ttf -framework SDL2_image \
  -framework OpenGL -framework AudioToolbox -framework CoreMIDI -framework CoreFoundation \
  -lsqlite3 -Wl,-rpath,@executable_path/../Frameworks \
  -o "$src/build-macos/linthesia"
printf 'Built: %s\n' "$src/build-macos/linthesia"
