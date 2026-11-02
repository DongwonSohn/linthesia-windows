#!/usr/bin/env bash
# Run the MIDI/text regression checks on macOS or Linux without SDL.
set -euo pipefail
src="$(cd "$(dirname "$0")/.." && pwd)"
check_dir="$(mktemp -d "${TMPDIR:-/tmp}/linthesia-core.XXXXXX")"
trap 'rm -rf "$check_dir"' EXIT
"${CXX:-c++}" -std=c++14 -I "$src/src" \
  "$src/tests/portable-core.cpp" "$src"/src/libmidi/*.cpp -o "$check_dir/portable-core"
"$check_dir/portable-core" "$@"
