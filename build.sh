#!/usr/bin/env bash
# Builds the DSP engine, its tests and the headless tools.
#
# This needs no C++ SDK: on Windows, where this project is developed, the host
# has no MSVC C++ toolchain (Visual Studio 2022 is installed without the
# VC/Tools/MSVC component), so the build uses zig's bundled clang + libc++
# through the `python-zig` launcher, which needs no admin rights:
#     uv tool install ziglang
#
# On Linux/macOS any recent clang or g++ works:
#     CXX_ZIG="clang++" bash build.sh
#
# A CMake build (CMakeLists.txt) also exists for machines with a standard
# toolchain; the preset bank, the WAV renderer and the tests are the same either
# way.
set -euo pipefail

ROOT="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
cd "$ROOT"

CXX="${CXX_ZIG:-python-zig}"
BUILD_DIR="${BUILD_DIR:-build}"
EXE=""
case "$(uname -s)" in
    MINGW*|MSYS*|CYGWIN*) EXE=".exe" ;;
esac
CXXFLAGS_COMMON="-std=c++20 -Isrc -Wall -Wextra -Wno-nullability-completeness -Wno-unused-parameter"

mkdir -p "$BUILD_DIR"

SOURCES=$(ls src/sh101/*.cpp src/plugin/*.cpp)
TESTS=$(ls tests/*.cpp)

echo "== building test runner =="
# shellcheck disable=SC2086
"$CXX" c++ $CXXFLAGS_COMMON -O2 $SOURCES $TESTS -o "$BUILD_DIR/sh101_tests$EXE"

echo "== building headless renderer =="
# shellcheck disable=SC2086
"$CXX" c++ $CXXFLAGS_COMMON -O2 $SOURCES tools/render_cli.cpp -o "$BUILD_DIR/sh101_render$EXE"

echo "== building calibration tools =="
# shellcheck disable=SC2086
"$CXX" c++ $CXXFLAGS_COMMON -O2 $SOURCES tools/bench_cli.cpp -o "$BUILD_DIR/sh101_bench$EXE"
# shellcheck disable=SC2086
"$CXX" c++ $CXXFLAGS_COMMON -O2 src/sh101/SH101VCO.cpp tools/alias_probe.cpp -o "$BUILD_DIR/alias_probe$EXE"

echo "build ok: $BUILD_DIR/sh101_tests$EXE, $BUILD_DIR/sh101_render$EXE, $BUILD_DIR/sh101_bench$EXE, $BUILD_DIR/alias_probe$EXE"
