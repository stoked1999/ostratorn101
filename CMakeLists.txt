cmake_minimum_required(VERSION 3.22)
project(SH101Emulation VERSION 0.1.0 LANGUAGES CXX)

# ---------------------------------------------------------------------------
# SH-101 emulation — build file.
#
# Targets:
#   sh101_core    static library: the complete DSP voice + host adapter
#   sh101_tests   headless validation test runner (the acceptance evidence)
#   sh101_render  headless WAV renderer for auditioning/measurement
#   sh101_bench   CPU/realtime benchmark
#   alias_probe   oscillator alias measurement
#   SH101Plugin   VST3/Standalone plugin (only when SH101_BUILD_VST3=ON and a
#                 JUCE tree is available; see docs/VST3.md)
#
# On the development machine no MSVC C++ toolchain is installed, so the project
# is built with zig's clang/libc++ through build.sh.  This CMake file is the
# canonical build for machines with a normal compiler (MSVC, clang or GCC) and
# is also what a JUCE/VST3 build will use.
# ---------------------------------------------------------------------------

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

if(NOT CMAKE_BUILD_TYPE AND NOT CMAKE_CONFIGURATION_TYPES)
    set(CMAKE_BUILD_TYPE Release CACHE STRING "" FORCE)
endif()

option(SH101_BUILD_TESTS "Build the validation test runner" ON)
option(SH101_BUILD_TOOLS "Build the headless render/benchmark/calibration tools" ON)
option(SH101_BUILD_VST3 "Build the VST3 plugin (requires JUCE)" OFF)
option(SH101_ENABLE_SANITIZERS "Enable ASan/UBSan in Debug builds" OFF)

# ---- Core DSP + host adapter ----------------------------------------------
file(GLOB SH101_DSP_SOURCES CONFIGURE_DEPENDS "${CMAKE_CURRENT_SOURCE_DIR}/src/sh101/*.cpp")
file(GLOB SH101_PLUGIN_SOURCES CONFIGURE_DEPENDS "${CMAKE_CURRENT_SOURCE_DIR}/src/plugin/*.cpp")

add_library(sh101_core STATIC ${SH101_DSP_SOURCES} ${SH101_PLUGIN_SOURCES})
target_include_directories(sh101_core PUBLIC "${CMAKE_CURRENT_SOURCE_DIR}/src")

if(MSVC)
    target_compile_options(sh101_core PRIVATE /W4 /permissive-)
else()
    target_compile_options(sh101_core PRIVATE -Wall -Wextra -Wpedantic)
endif()

if(SH101_ENABLE_SANITIZERS AND NOT MSVC)
    target_compile_options(sh101_core PUBLIC -fsanitize=address,undefined -fno-omit-frame-pointer)
    target_link_options(sh101_core PUBLIC -fsanitize=address,undefined)
endif()

# ---- Validation tests -----------------------------------------------------
if(SH101_BUILD_TESTS)
    enable_testing()
    file(GLOB SH101_TEST_SOURCES CONFIGURE_DEPENDS "${CMAKE_CURRENT_SOURCE_DIR}/tests/*.cpp")
    add_executable(sh101_tests ${SH101_TEST_SOURCES})
    target_link_libraries(sh101_tests PRIVATE sh101_core)
    target_include_directories(sh101_tests PRIVATE "${CMAKE_CURRENT_SOURCE_DIR}/tests")
    add_test(NAME validation COMMAND sh101_tests)
endif()

# ---- Headless tools -------------------------------------------------------
if(SH101_BUILD_TOOLS)
    add_executable(sh101_render tools/render_cli.cpp)
    target_link_libraries(sh101_render PRIVATE sh101_core)

    add_executable(sh101_bench tools/bench_cli.cpp)
    target_link_libraries(sh101_bench PRIVATE sh101_core)

    add_executable(alias_probe tools/alias_probe.cpp)
    target_link_libraries(alias_probe PRIVATE sh101_core)

    add_executable(adapter_probe tools/adapter_probe.cpp)
    target_link_libraries(adapter_probe PRIVATE sh101_core)
endif()

# ---- VST3 / Standalone plugin (optional) ----------------------------------
if(SH101_BUILD_VST3)
    if(NOT DEFINED JUCE_DIR)
        message(FATAL_ERROR "Set JUCE_DIR to a JUCE checkout, e.g. -DJUCE_DIR=C:/JUCE")
    endif()
    add_subdirectory("${JUCE_DIR}" "${CMAKE_BINARY_DIR}/JUCE")
    add_subdirectory(src/plugin/juce)
endif()
