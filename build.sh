#!/usr/bin/env bash
# Build script for the SH-101 model on this machine.
#
# The host has no MSVC C++ toolchain installed (Visual Studio 2022 is present
# without the VC/Tools/MSVC component), so the build uses zig's bundled clang +
# libc++ through the `python-zig` launcher, which needs no admin rights:
#     uv tool install ziglang cmake ninja
#
# A regular CMake build (CMakeLists.txt) also exists for machines that have a
# standard toolchain.
set -euo pipefail

ROOT="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
cd "$ROOT"

CXX_ZIG="${CXX_ZIG:-python-zig}"
BUILD_DIR="${BUILD_DIR:-build}"
CXXFLAGS_COMMON="-std=c++20 -Isrc -Wall -Wextra -Wno-nullability-completeness -Wno-unused-parameter"

mkdir -p "$BUILD_DIR"

SOURCES=$(ls src/sh101/*.cpp src/plugin/*.cpp)
TESTS=$(ls tests/*.cpp)

echo "== building test runner =="
# shellcheck disable=SC2086
"$CXX_ZIG" c++ $CXXFLAGS_COMMON -O2 $SOURCES $TESTS -o "$BUILD_DIR/sh101_tests.exe"

echo "== building headless renderer =="
# shellcheck disable=SC2086
"$CXX_ZIG" c++ $CXXFLAGS_COMMON -O2 $SOURCES tools/render_cli.cpp -o "$BUILD_DIR/sh101_render.exe"

echo "== building calibration tools =="
# shellcheck disable=SC2086
"$CXX_ZIG" c++ $CXXFLAGS_COMMON -O2 $SOURCES tools/bench_cli.cpp -o "$BUILD_DIR/sh101_bench.exe"
python-zig c++ $CXXFLAGS_COMMON -O2 src/sh101/SH101VCO.cpp tools/alias_probe.cpp -o "$BUILD_DIR/alias_probe.exe"

echo "build ok: $BUILD_DIR/sh101_tests.exe, $BUILD_DIR/sh101_render.exe, $BUILD_DIR/sh101_bench.exe"
