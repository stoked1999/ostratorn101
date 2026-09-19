#!/usr/bin/env bash
# Configure-only test for the plugin build (debugging aid for compiler detection).
set -uo pipefail
export PATH="$HOME/.local/bin:$PATH"

winpath() {
    case "$1" in
        /[a-zA-Z]/*) printf '%s:%s\n' "$(printf '%s' "${1:1:1}" | tr 'a-z' 'A-Z')" "${1:2}" ;;
        *) printf '%s\n' "$1" ;;
    esac
}

ROOT="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
JUCE_DIR="${1:-/c/Users/bbhal/juce-src}"
BUILD_DIR="$ROOT/build-plugin"
TOOLS="$ROOT/tools"

rm -rf "$BUILD_DIR"
echo "== configure attempt: wrapper scripts as compilers =="
cmake -S "$(winpath "$ROOT")" -B "$(winpath "$BUILD_DIR")" -G Ninja \
    -DCMAKE_BUILD_TYPE=Release \
    -DCMAKE_C_COMPILER="$(winpath "$TOOLS/zig-cc.bat")" \
    -DCMAKE_CXX_COMPILER="$(winpath "$TOOLS/zig-cxx.bat")" \
    -DCMAKE_AR="$(winpath "$TOOLS/zig-ar.bat")" \
    -DCMAKE_RANLIB="$(winpath "$TOOLS/zig-ranlib.bat")" \
    -DCMAKE_RC_COMPILER="$(winpath "$TOOLS/zig-rc.bat")" \
    -DCMAKE_MAKE_PROGRAM="$(winpath "$HOME/.local/bin/ninja.exe")" \
    -DSH101_BUILD_TESTS=OFF \
    -DSH101_BUILD_TOOLS=OFF \
    -DSH101_BUILD_VST3=ON \
    -DJUCE_DIR="$(winpath "$JUCE_DIR")" 2>&1 | tail -25
echo "== configure exit: ${PIPESTATUS[0]} =="
