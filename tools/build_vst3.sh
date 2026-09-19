#!/usr/bin/env bash
# Builds the VST3 with zig's clang + a JUCE checkout, then installs the bundle
# into a user-writable VST3 folder (no admin rights required).
#
# Usage:  bash tools/build_vst3.sh [juce-dir]
#
# Notes:
#  * Every path handed to a native Windows program (cmake, ninja, zig) must be a
#    native path: MSYS path translation is disabled on this host, so /c/... would
#    be taken literally.  winpath() converts.
#  * JUCE rejects MinGW by design (juce_TargetPlatform.h).  This build is an
#    experiment with that one policy check patched out; if it fails, install the
#    MSVC C++ workload and build with the normal CMake flow (docs/VST3.md).
#  * The system VST3 folder (C:\Program Files\Common Files\VST3) is not writable
#    without admin, so the bundle is installed to ~/VST3 and Live is pointed at
#    it with "Use VST3 Plug-In Custom Folder".
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
INSTALL_DIR="${INSTALL_DIR:-$HOME/VST3}"

if [ ! -d "$JUCE_DIR/modules" ]; then
    echo "error: no JUCE checkout at $JUCE_DIR"
    exit 1
fi

echo "== configuring (JUCE: $JUCE_DIR) =="
cmake -S "$(winpath "$ROOT")" -B "$(winpath "$BUILD_DIR")" -G Ninja \
    -DCMAKE_BUILD_TYPE=Release \
    -DCMAKE_C_FLAGS="${EXTRA_CFLAGS:-}" \
    -DCMAKE_CXX_FLAGS="${EXTRA_CFLAGS:-}" \
    -DCMAKE_C_COMPILER="$(winpath "$TOOLS/zig-cc.bat")" \
    -DCMAKE_CXX_COMPILER="$(winpath "$TOOLS/zig-cxx.bat")" \
    -DCMAKE_AR="$(winpath "$TOOLS/zig-ar.bat")" \
    -DCMAKE_RANLIB="$(winpath "$TOOLS/zig-ranlib.bat")" \
    -DCMAKE_RC_COMPILER="$(winpath "$TOOLS/zig-rc.bat")" \
    -DCMAKE_MAKE_PROGRAM="$(winpath "$HOME/.local/bin/ninja.exe")" \
    -DSH101_BUILD_TESTS=OFF \
    -DSH101_BUILD_TOOLS=OFF \
    -DSH101_BUILD_VST3=ON \
    -DJUCE_DIR="$(winpath "$JUCE_DIR")"
rc=$?
if [ $rc -ne 0 ]; then
    echo "== configure failed ($rc) =="
    exit $rc
fi

echo "== building VST3 =="
cmake --build "$(winpath "$BUILD_DIR")" --target SH101Plugin_VST3 -- -j 8
rc=$?
if [ $rc -ne 0 ]; then
    echo "== build failed ($rc) =="
    exit $rc
fi

ARTEFACTS="$BUILD_DIR/src/plugin/juce/SH101Plugin_artefacts/Release/VST3"
BUNDLE="$ARTEFACTS/SH-101.vst3"
if [ ! -d "$BUNDLE" ]; then
    echo "== expected bundle not found: $BUNDLE =="
    ls -R "$ARTEFACTS" 2>/dev/null | head -20
    exit 1
fi

mkdir -p "$INSTALL_DIR"
rm -rf "$INSTALL_DIR/SH-101.vst3"
cp -r "$BUNDLE" "$INSTALL_DIR/"
echo "== installed: $INSTALL_DIR/SH-101.vst3 =="
find "$INSTALL_DIR/SH-101.vst3" -type f | head -10
