#!/usr/bin/env bash
# Builds the VST3 with MSVC (the supported JUCE configuration) and installs the
# bundle into a user-writable VST3 folder.
#
# Requires: Visual Studio 2022 with the "Desktop development with C++" workload
# (which brings cl.exe, MSBuild and a Windows SDK).
#
# Usage:  bash tools/build_vst3_msvc.sh [juce-dir]
set -uo pipefail

export PATH="$HOME/.local/bin:$PATH"   # cmake/ninja come from uv tool installs

winpath() {
    case "$1" in
        /[a-zA-Z]/*) printf '%s:%s\n' "$(printf '%s' "${1:1:1}" | tr 'a-z' 'A-Z')" "${1:2}" ;;
        *) printf '%s\n' "$1" ;;
    esac
}

ROOT="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
JUCE_DIR="${1:-/c/Users/bbhal/juce-src}"
BUILD_DIR="$ROOT/build-plugin-msvc"
INSTALL_DIR="${INSTALL_DIR:-$HOME/VST3}"
TARGET_PLATFORM_HEADER="$JUCE_DIR/modules/juce_core/system/juce_TargetPlatform.h"
BACKUP="$TARGET_PLATFORM_HEADER.orig"

echo "== checking toolchain =="
COMMUNITY_TOOLS="/c/Program Files/Microsoft Visual Studio/2022/Community/VC/Tools/MSVC"
BUILDTOOLS_TOOLS="/c/Program Files (x86)/Microsoft Visual Studio/2022/BuildTools/VC/Tools/MSVC"
GENERATOR_INSTANCE=""

if [ -d "$BUILDTOOLS_TOOLS" ]; then
    # Prefer the Build Tools instance: the Community install on this machine has
    # no C++ toolset, and with both present CMake may pick the wrong instance.
    GENERATOR_INSTANCE="C:/Program Files (x86)/Microsoft Visual Studio/2022/BuildTools"
    echo "using Build Tools instance: $GENERATOR_INSTANCE"
elif [ -d "$COMMUNITY_TOOLS" ]; then
    GENERATOR_INSTANCE="C:/Program Files/Microsoft Visual Studio/2022/Community"
    echo "using Community instance: $GENERATOR_INSTANCE"
else
    echo "error: no MSVC C++ toolset found. Install the 'Desktop development with C++'"
    echo "       workload (or the VCTools Build Tools workload), then re-run."
    exit 1
fi
if [ ! -d "/c/Program Files (x86)/Windows Kits/10/Include" ]; then
    echo "warning: no Windows SDK found under Program Files (x86)\\Windows Kits - the"
    echo "         build will fail at the first windows.h include."
fi

# The MinGW experiment patches JUCE's target platform header.  MSVC does not need
# them, so restore the pristine file when the backup is present.
if grep -q "PATCHED LOCALLY (SH-101 project)" "$TARGET_PLATFORM_HEADER" 2>/dev/null; then
    if [ -f "$BACKUP" ]; then
        cp "$BACKUP" "$TARGET_PLATFORM_HEADER"
        echo "== restored pristine juce_TargetPlatform.h from backup =="
    else
        echo "warning: JUCE target-platform header still carries the MinGW patch and no"
        echo "         backup exists; restore it manually if the build misbehaves."
    fi
fi

echo "== configuring (Visual Studio generator) =="
cmake -S "$(winpath "$ROOT")" -B "$(winpath "$BUILD_DIR")" \
    -G "Visual Studio 17 2022" -A x64 \
    -DCMAKE_GENERATOR_INSTANCE="$GENERATOR_INSTANCE" \
    -DSH101_BUILD_TESTS=OFF \
    -DSH101_BUILD_TOOLS=OFF \
    -DSH101_BUILD_VST3=ON \
    -DJUCE_DIR="$(winpath "$JUCE_DIR")"
rc=$?
if [ $rc -ne 0 ]; then
    echo "== configure failed ($rc) =="
    exit $rc
fi

echo "== building VST3 (Release) =="
cmake --build "$(winpath "$BUILD_DIR")" --config Release --target SH101Plugin_VST3
rc=$?
if [ $rc -ne 0 ]; then
    echo "== build failed ($rc) =="
    exit $rc
fi

ARTEFACTS="$BUILD_DIR/src/plugin/juce/SH101Plugin_artefacts/Release/VST3"
BUNDLE="$ARTEFACTS/SH-101.vst3"
if [ ! -d "$BUNDLE" ]; then
    echo "== expected bundle not found: $BUNDLE =="
    find "$BUILD_DIR" -name "*.vst3" 2>/dev/null | head -5
    exit 1
fi

mkdir -p "$INSTALL_DIR"
rm -rf "$INSTALL_DIR/SH-101.vst3"
cp -r "$BUNDLE" "$INSTALL_DIR/"
echo "== installed: $INSTALL_DIR/SH-101.vst3 =="
find "$INSTALL_DIR/SH-101.vst3" -type f
