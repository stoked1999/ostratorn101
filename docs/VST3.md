# VST3 plugin: build, install and verification

## Status: built, installed and verified

`C:\Users\bbhal\VST3\SH-101.vst3` — built with MSVC (Build Tools 14.44.35207 +
Windows SDK 10.0.26100), JUCE 8.0.15.

The plugin is verified by `tools/vst3_host_test.cpp`, which loads the installed
bundle through **JUCE's own plugin-hosting classes** (the same path a host
application uses) and checks the things a host needs:

```
plugins found: 1
  name='SH-101' manufacturer='SH101 Emulation Project' instrument=1 inputs=0 outputs=0
instantiated: 'SH-101' hasEditor=1
SH-101 parameters reported: 34 of 2115 total (the rest are host-side MIDI CC automation entries)
output channels: 2
peak: silence=0.000052 note=0.521147 tail=0.194319
editor: 600x700 (wrapper contains 0 juce children)
RESULT: plugin loads, accepts MIDI, produces audio and creates its editor
```

Run it yourself:

```bash
./build-plugin-msvc/sh101_host_test_artefacts/Release/sh101_host_test.exe "C:/Users/bbhal/VST3/SH-101.vst3"
```

The bundle itself is checked by `tools/verify_vst3.bat`: correct VST3 layout
(`Contents/x86_64-win/SH-101.vst3`), `Contents/Resources/moduleinfo.json`
declaring an Instrument/Synth class, and the DLL exporting the `GetPluginFactory`
entry point.

## Using it in Ableton Live

`C:\Program Files\Common Files\VST3` (Live's default VST3 system folder) is not
writable without admin — verified — so the plugin lives in a user folder and Live
is pointed at it:

1. Live → **Settings → Plug-Ins**
2. Enable **Use VST3 Plug-In Custom Folder**
3. Browse to `C:\Users\bbhal\VST3`
4. Press **Rescan Plug-Ins** (with "Use VST3 Plug-In System Folder" left on as
   well; the custom folder must contain only VST3s, which is why it is dedicated)

The plugin then appears in Live's browser as **SH-101** under VST3.

Alternative: copy `C:\Users\bbhal\VST3\SH-101.vst3` into
`C:\Program Files\Common Files\VST3` from an elevated Explorer window and leave
the custom folder off.

### What you get, and what you don't (yet)

* 34 automatable parameters with the original SH-101 ranges and tapers
  (`src/sh101/Params.h`), defaulting to the engine's reference patch.
* A real editor window (600x700): one labelled rotary per parameter, grouped like
  the front panel (LFO, VCO, source mixer, VCF, ENV, VCA/performance,
  arpeggiator/sequencer). It has been compiled and instantiated, but never seen
  by a human — expect layout tweaks to be wanted.
* Enumerated controls (`vcoRange`, `subMode`, `lfoWave`, `pwmSource`,
  `envTrigger`, `vcaMode`, `portamentoMode`, `arpMode`, `arpOn`, `seqOn`) are
  continuous 0..1 floats rather than named choices; the DSP taper is correct.
* Host tempo is not yet wired to the arpeggiator/sequencer (`setArpSyncToHost` /
  `hostClockTick` exist and are tested; the `AudioPlayHead` side is not connected).
* No preset manager; state save/load is APVTS-based.

## Rebuilding

```bash
bash tools/build_vst3_msvc.sh     # configures + builds + installs (VS generator)
MSYS_NO_PATHCONV=1 cmd /c "C:\Users\bbhal\sh101\tools\build_vst3_msvc.bat"   # vcvars + Ninja
```

Two build paths exist because CMake's "Visual Studio 17 2022" generator cannot
use this machine's Build Tools instance (it is not registered in the VS Installer
database, so CMake reports "the instance is not known to the Visual Studio
Installer"). The `.bat` loads `vcvars64.bat` and drives Ninja instead — that is
the one that works here. The `.sh` is kept for machines with a registered VS
instance.

## Why there is no MinGW build

JUCE cannot be built with MinGW/clang, by design:

1. `juce_core/system/juce_TargetPlatform.h` hard-errors on MinGW.
2. The same header sets `JUCE_64BIT` only under `_MSC_VER`, so a MinGW build
   compiles as 32-bit internally and every `pointer_sized_uint` cast fails.
3. The Windows GUI/graphics code uses SDK enumerators mingw-w64 does not ship
   (`D2D1_SATURATION_PROP_SATURATION`, `CaretPosition_*`).

A MinGW experiment (zig clang + libc++, with 1 and 2 patched locally in the JUCE
checkout) got `juceaide` from 24 errors down to 5 before it was stopped as a
dead end: more layers (Direct2D, UIA, WASAPI) remained, and a MinGW-built VST3
would link a different C++ runtime than the MSVC-built host anyway.
`tools/build_vst3.sh` is kept as that experiment; it is not the supported path.

## Toolchain notes for this machine

* Visual Studio 2022 Community is installed **without** the C++ workload, and the
  VS Installer's `modify` path fails here (exit code 1 right after manifest
  verification). The working route was the standalone bootstrapper:
  `vs_BuildTools.exe --passive --wait --add Microsoft.VisualStudio.Workload.VCTools
  --includeRecommended` (`tools/install_buildtools.ps1`), which installed MSVC and
  the Windows SDK into `C:\Program Files (x86)\Microsoft Visual Studio\2022\BuildTools`.
* JUCE lives at `C:\Users\bbhal\juce-src` (tag 8.0.15, cloned shallow).
* The uv-installed zig toolchain (`build.sh`) remains what builds and tests the
  DSP; it needs no MSVC.
