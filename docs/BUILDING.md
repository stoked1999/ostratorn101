# Building ÖstraTorn101

Two independent build paths. You do **not** need the plugin toolchain to read or
run the DSP engine — the engine, its tests and the WAV renderer build anywhere a
C++20 compiler exists.

## 1. DSP engine, tests and headless tools (no SDK, any platform)

Needs only a C++20 compiler. On Windows this project uses zig's bundled clang +
libc++ through the `python-zig` launcher, because the development host has no
MSVC C++ toolchain — and it needs no admin rights:

```bash
uv tool install ziglang          # one-off; adds python-zig to ~/.local/bin
bash build.sh                    # builds into build/
```

On Linux/macOS any recent compiler works:

```bash
CXX_ZIG="clang++" bash build.sh  # or CXX_ZIG="g++"
```

`build.sh` honours `CXX_ZIG` and `BUILD_DIR`, and adds `.exe` only on Windows.
What you get in `build/`:

| Binary | What it does |
| --- | --- |
| `sh101_tests` | the engine acceptance suite — 78 cases / 3518 checks |
| `sh101_render` | offline WAV renderer: `--list-presets`, `--preset "Acid Bass" out.wav` |
| `sh101_bench` | CPU cost per block (used for the real-time headroom figures) |
| `alias_probe` | oscillator aliasing measurement |

```bash
./build/sh101_tests                                  # the acceptance evidence
./build/sh101_render --list-presets
./build/sh101_render --preset "Acid Bass" renders/acid.wav
```

## 2. The VST3 plugin (Windows, MSVC + JUCE)

Needs: MSVC with the **Desktop development with C++** workload (Visual Studio
2022 Build Tools is enough — no IDE required), CMake ≥ 3.22, Ninja, and a
**JUCE 8.x** checkout.

```bat
git clone --branch 8.0.15 --depth 1 https://github.com/juce-framework/JUCE.git C:\JUCE
tools\build_vst3_msvc.bat
```

That script finds `vcvars64.bat`, CMake, Ninja and the JUCE checkout by itself.
If any of them lives somewhere unusual, point at it from the environment instead
of editing the file — it reads `VCVARS`, `CMAKE`, `NINJA`, `JUCE_DIR`, `SRC`
and `BUILD`, and prints what it resolved before configuring:

```bat
set JUCE_DIR=D:\src\JUCE
set CMAKE=C:\tools\cmake\bin\cmake.exe
tools\build_vst3_msvc.bat
```

Plain CMake works too:

```bash
cmake -S . -B build-plugin-msvc -G Ninja -DCMAKE_BUILD_TYPE=Release \
      -DSH101_BUILD_VST3=ON -DJUCE_DIR=C:/JUCE
cmake --build build-plugin-msvc --target SH101Plugin_VST3 sh101_host_test sh101_editor_test
```

It uses Ninja rather than the "Visual Studio 17 2022" generator on purpose: that
generator requires the build-tools instance to be registered in the VS Installer
database, which a standalone Build Tools install is not. `vcvars64.bat` + Ninja
avoids the problem.

**The JUCE patches described in [`docs/VST3.md`](VST3.md) are not required to
build.** `JucePlugin_Vst3ClassName` is deliberately unset in
`src/plugin/juce/CMakeLists.txt` — stock JUCE 8.0.15 compiles the plugin as-is;
the patch only exists to experiment with a non-ASCII product name.

### Verifying your build

```bash
# loads the built bundle through JUCE's own hosting classes, same path a DAW uses
./build-plugin-msvc/sh101_host_test_artefacts/Release/sh101_host_test.exe <path-to>/OstraTorn101.vst3

# builds the plugin's own editor, snapshots it to renders/plugin_ui.png, and
# checks the panel layout, the preset library and the switch→DSP path
./build-plugin-msvc/src/plugin/juce/sh101_editor_test_artefacts/Release/sh101_editor_test.exe renders/plugin_ui.png
```

`tools/install_vst3_all.ps1` and the paths inside `docs/VST3.md` refer to the
author's machine (Ableton Live's default VST3 folder) — you do not need either
to build or to use the plugin: copy the built `OstraTorn101.vst3` folder into
your own VST3 folder and rescan in your host.

## Where the code is

```
src/sh101/        the voice — one class per block, named after the object list in
                  docs/engineering-brief.md; SH101Engine.cpp walks them in the
                  brief's documented processing order and is the place to start
src/plugin/       host-facing layer: SH101HostAdapter (MIDI, automation, blocks),
                  src/plugin/juce/ (VST3 shell: processor, panel editor)
tests/            the acceptance suite (also the specification, case by case)
tools/            render_cli, bench_cli, alias_probe, adapter_probe, editor_test,
                  vst3_host_test, verify_renders.py, check_no_alloc.py
docs/             engineering-brief.md (the requirements), VALIDATION.md (measured
                  results), APPROXIMATIONS.md (every known deviation from the hardware)
```

Reading order that matches how it was built: `docs/engineering-brief.md` →
`src/sh101/SH101Engine.cpp` → the block implementations → `tests/` →
`docs/VALIDATION.md`.
