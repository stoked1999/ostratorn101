# VST3 wrapper status and build steps

## Status: written, but not compiled in this environment

The brief's first-coding-task order ends with "9. VST3 wrapper". The wrapper
exists in `src/plugin/juce/`:

| File | Purpose |
| --- | --- |
| `PluginProcessor.h/.cpp` | `juce::AudioProcessor`; owns a `sh101::SH101HostAdapter`, exposes all 34 SH-101 controls as normalized `AudioParameterFloat`s, handles state save/load and MIDI |
| `PluginEditor.h/.cpp` | Minimal editor: one labelled control per parameter, grouped like the front panel. No branding, no extra features |
| `CMakeLists.txt` | `juce_add_plugin` target (VST3 + Standalone) |

**It has not been compiled or run.** The development machine has Visual Studio
2022 installed *without* the "Desktop development with C++" workload (no
`VC/Tools/MSVC`, no Windows SDK) and no JUCE checkout, and no admin rights to
install either. Everything the wrapper depends on *is* compiled and tested:

* the complete DSP voice (`src/sh101/`), validated by 57 test cases, and
* the host-facing layer the wrapper is a thin shell over
  (`src/plugin/SH101HostAdapter.*`), covered by `tests/test_host_adapter.cpp`
  (MIDI note on/off, velocity-0 note off, sustain pedal, All Notes Off, pitch
  bend range, normalized automation, host-clock sync, multi-channel rendering,
  note events inside one host block).

Treat the JUCE files as unverified code until a build succeeds.

## Building it (once a toolchain is present)

1. Install a C++ toolchain: Visual Studio 2022 Build Tools with
   *Desktop development with C++* (or LLVM/clang + the Windows SDK).
2. Get JUCE (7.0.9 or newer):

   ```
   git clone --depth 1 --branch 7.0.12 https://github.com/juce-framework/JUCE.git C:/JUCE
   ```

   Note: JUCE for VST3 on Windows wants a normal MSVC/Windows-SDK toolchain.
   The zig/libc++ setup used for the DSP build here is not a supported JUCE
   configuration.
3. Configure and build:

   ```
   cmake -S . -B build-plugin -G "Visual Studio 17 2022" -A x64 \
         -DSH101_BUILD_VST3=ON -DJUCE_DIR=C:/JUCE
   cmake --build build-plugin --config Release
   ```

   The VST3 ends up under `build-plugin/src/plugin/juce/SH101Plugin_artefacts/Release/VST3/`.

## What the wrapper does per block

1. Reads all 34 parameters (one `setParametersNormalized()` call, one parameter
   commit — not 34).
2. Feeds MIDI messages to the adapter in order, *before* rendering. This matters:
   the engine consumes ordered gate transitions, so a note that starts and ends
   inside one host block is still played and released. A state-only gate
   interface fails this case (it was a real bug — see
   `tests/test_host_adapter.cpp::adapter_note_inside_one_block_is_released`).
3. Renders mono into channel 0 and copies it to the remaining channels, so the
   plugin cannot alter the default SH-101 sound with stereo processing.

## Known gaps to close during the first plugin build

* `PluginEditor.cpp` is layout-only and has never been seen by a compiler.
* Enumerated controls (`vcoRange`, `subMode`, `lfoWave`, `pwmSource`,
  `envTrigger`, `vcaMode`, `portamentoMode`, `arpMode`, `arpOn`, `seqOn`) are
  exposed as continuous 0..1 floats; a `juce::AudioParameterChoice` wrapper would
  present them better. The DSP taper is already correct in `Params.h`.
* Host transport/`AudioPlayHead` tempo is not yet wired to
  `setArpSyncToHost()`/`hostClockTick()`; the engine side exists and is tested.
* No preset manager; state save/load is APVTS-based only.
