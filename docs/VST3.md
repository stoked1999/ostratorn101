# VST3 plugin: build, install and verification

## Status: built, installed and verified

Built with MSVC (Build Tools 14.44.35207 + Windows SDK 10.0.26100) against JUCE
8.0.15. The bundle is installed in three places:

| Location | Contents |
| --- | --- |
| `C:\Users\bbhal\VST3\SH-101.vst3` | **current build** (the one the tests below ran against) |
| `C:\Program Files\Common Files\VST3\SH-101.vst3` | Live's default VST3 folder — the *previous* build until an elevated copy is approved |
| `C:\Program Files (x86)\Common Files\VST3\SH-101.vst3` | same, for the 32-bit folder (Live 10+ does not scan it) |

Refreshing the two system copies needs one admin approval:

```bash
powershell -NoProfile -ExecutionPolicy Bypass -File tools/install_vst3_all.ps1   # one UAC prompt, both folders
```

`tools/install_vst3.ps1 -Destination '<folder>'` installs to one folder;
`tools/install_vst3_inner.ps1` is the elevated half. If no one is at the keyboard
to accept the prompt, Live can instead be pointed at `C:\Users\bbhal\VST3`
(see below) — that copy is always written by the build without admin.

## Verification

`tools/vst3_host_test.cpp` loads the installed bundle through **JUCE's own
plugin-hosting classes** (the same path a host application uses) and checks what
a host needs:

```
plugins found: 1
  name='SH-101' manufacturer='SH101 Emulation Project' instrument=1
instantiated: 'SH-101' hasEditor=1
SH-101 parameters reported: 34 of 2116 total (the rest are host-side MIDI CC automation entries)
output channels: 2
peak: silence=0.000032 note=0.451494 tail=0.171046
editor: 908x576 (wrapper contains 0 juce children)
RESULT: plugin loads, accepts MIDI, produces audio and creates its editor
```

```bash
./build-plugin-msvc/sh101_host_test_artefacts/Release/sh101_host_test.exe "C:/Users/bbhal/VST3/SH-101.vst3"
```

VST3 hosting cannot inspect the editor's *contents* — JUCE hosts a plugin's view
in a native child window, so the component a host holds has no JUCE children and
renders nothing off-screen (measured: 0 children, empty snapshot). The panel and
the preset wiring are therefore verified by `tools/editor_test.cpp`, which builds
the plugin's own editor directly, snapshots it to `renders/plugin_ui.png` and
checks that the panel really draws:

```bash
./build-plugin-msvc/src/plugin/juce/sh101_editor_test_artefacts/Release/sh101_editor_test.exe renders/plugin_ui.png
```

```
  ok: the preset bank is exposed as the plugin's programs        (21 programs)
  ok: program names are labelled                                 ("Bass: Acid Bass")
  ok: a fresh instance starts on the reference patch
  ok: every control matches the selected preset
  ok: the preset's sequence pattern reached the engine           (8 steps, preset notes)
  rendered peak after a preset load: 0.3831
  ok: a loaded preset makes sound in the plugin
  editor size: 908 x 576, direct children: 76
  ok: every control was laid out (the blank-window bug)
  first slider read-out: '5.00Hz' (formatter set)
  ok: a fader's value is shown in engineering units, not as a raw 0..1 number
  drawn pixels: 77.7% of the window, accent pixels: 1991
  ok: the panel paints a substantial part of the window
  control-area bands containing accents: 4 of 4
  ok: controls and headers are spread across the whole panel
  ok: the editor snapshot was written to renders/plugin_ui.png
PASS: 0 failure(s)
```

## Using it in Ableton Live

The plugin is in Live's **default** VST3 folder, so no settings are needed:

1. Open Live (restart it if it was already running)
2. Browser → **Plug-Ins → VST3 → SH-101**
3. Drag it onto a MIDI track, arm the track and play

If it does not appear: **Settings → Plug-Ins → Rescan Plug-Ins**, with "Use VST3
Plug-In System Folder" enabled.

Alternative without admin: enable **Use VST3 Plug-In Custom Folder** and point it
at `C:\Users\bbhal\VST3`, which always holds the newest build.

## What you get

* 34 automatable parameters with the original SH-101 ranges and tapers
  (`src/sh101/Params.h`), defaulting to the engine's reference patch.
* 10 of them are **choice parameters** (range, sub mode, LFO wave, PWM source,
  trigger mode, VCA mode, portamento mode, arp mode, arp on, seq on), so the host
  and the panel show real position names rather than anonymous numbers.
* A panel UI (908×576): a fader per continuous control with an amber cap and an
  engineering-unit read-out (Hz, ms, %, oct, ct), switch-like controls as
  drop-downs, sections in signal order (LFO, VCO, source mixer, VCF, ENV,
  VCA/performance, arpeggiator/sequencer), and a preset bar.
* A 21-patch preset bank (`src/sh101/Presets.h`): bass, lead, pad/strings, pluck,
  percussion, FX and sequenced patches. It is the plugin's **program list**, so
  the host's own preset menu and the panel's selector stay in step. Sequenced
  presets also carry their pattern into the engine's sequence memory.
* Host tempo is not yet wired to the arpeggiator/sequencer (`setArpSyncToHost` /
  `hostClockTick` exist and are tested; the `AudioPlayHead` side is not connected).

## Two UI bugs this round found and fixed

Both were invisible to every previous test and are now covered by
`tools/editor_test.cpp`:

1. **Blank window.** The editor's constructor called `setSize()` *before* adding
   its controls, so `resized()` ran with nothing to lay out and every control
   stayed at 0×0 — an empty panel. Sizing now happens last, and the test fails if
   any control has no size.
2. **Invisible faders.** `LookAndFeel_V4`'s slider layout handed a 56-pixel-high
   vertical fader a 17-pixel groove (and a zero-width read-out), so the custom
   `drawLinearSlider` returned early every time: controls were laid out and
   present, but nothing was drawn. `getSliderLayout` is now overridden. A
   secondary cause: `SliderAttachment` installs its own value→text formatter, so
   the panel's engineering-unit formatter has to be assigned *after* the
   attachment (otherwise every read-out showed a raw 0..1 number).

## Rebuilding

```bash
MSYS_NO_PATHCONV=1 cmd /c "C:\Users\bbhal\sh101\tools\build_vst3_msvc.bat"   # vcvars + Ninja: VST3, host test, editor test
bash tools/build_vst3_msvc.sh     # configures + builds + installs (VS generator; needs a registered VS instance)
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
