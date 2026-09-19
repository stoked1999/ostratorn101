# ÖstraTorn101 — VST3 build, install and verification

## Status: built, installed and verified

Built with MSVC (Build Tools 14.44.35207 + Windows SDK 10.0.26100) against JUCE
8.0.15. The bundle is `OstraTorn101.vst3`; the name a DAW displays is
**ÖstraTorn101** (see "Product name" below for why those two differ).

| Location | Contents |
| --- | --- |
| `C:\Users\bbhal\VST3\OstraTorn101.vst3` | **current build** (what the tests below ran against) |
| `C:\Program Files\Common Files\VST3\OstraTorn101.vst3` | Live's default VST3 folder — needs one admin approval per update |
| `C:\Program Files (x86)\Common Files\VST3\OstraTorn101.vst3` | same, in the 32-bit folder (Live 10+ does not scan it) |

```bash
# one UAC prompt, both system folders (removes bundles from earlier names)
powershell -NoProfile -ExecutionPolicy Bypass -File tools/install_vst3_all.ps1
```

## Verification

`tools/vst3_host_test.cpp` loads the installed bundle through **JUCE's own
plugin-hosting classes** — the same path a host application uses:

```
plugins found: 1
  name='OstraTorn101' manufacturer='OstraTorn Audio' instrument=1
instantiated: 'OstraTorn101' hasEditor=1
ÖstraTorn101 parameters reported: 34 of 2116 total (the rest are host-side MIDI CC automation entries)
output channels: 2
peak: silence=0.000122 note=0.518226 tail=0.196745
editor: 1440x646 (wrapper contains 0 juce children)
RESULT: plugin loads, accepts MIDI, produces audio and creates its editor
```

The silence peak is not a bug: it is the modelled output-stage noise floor
(−88 dBFS RMS), about 0.0001 on this scale, which is what an analogue output
amplifier actually does when nothing is playing.

```bash
./build-plugin-msvc/sh101_host_test_artefacts/Release/sh101_host_test.exe "C:/Users/bbhal/VST3/OstraTorn101.vst3"
```

VST3 hosting cannot inspect the editor's *contents* — JUCE hosts a plugin's view
in a native child window, so the component a host holds has no JUCE children and
renders nothing off-screen (measured: 0 children, empty snapshot).  The panel and
the preset library are therefore verified by `tools/editor_test.cpp`, which builds
the plugin's own editor, snapshots it to `renders/plugin_ui.png` and checks:

* the factory bank is the plugin's program list (43 programs, labelled);
* loading a program moves every control, and the *switch positions reach the DSP*
  (this is what caught JUCE's raw-parameter-value trap, below);
* a sequenced preset delivers its pattern to the engine;
* a loaded preset makes sound; the level meter sees it; standby silences it;
* a user preset round-trips through the library: saved to
  `%APPDATA%\ÖstraTorn101\Presets\<name>.otp101`, listed, loaded back with every
  value identical, and deleted;
* the panel lays out (0 controls with no size), paints, shows engineering units
  (`5.00Hz` and not `0.6858647`) and spreads its sections across the window.

```bash
./build-plugin-msvc/src/plugin/juce/sh101_editor_test_artefacts/Release/sh101_editor_test.exe renders/plugin_ui.png
```

## Using it in Ableton Live

The plugin is installed in Live's **default** VST3 folder, so no settings are
needed:

1. Open Live (restart it if it was already running)
2. Browser → **Plug-Ins → VST3 → ÖstraTorn101**
3. Drag it onto a MIDI track, arm the track and play

If it does not appear: **Settings → Plug-Ins → Rescan Plug-Ins**, with "Use VST3
Plug-In System Folder" enabled.  Alternative without admin: enable **Use VST3
Plug-In Custom Folder** and point it at `C:\Users\bbhal\VST3`, which always holds
the newest build.

## The panel

1280×646 (or whatever the host gives it), laid out like hardware:

```
title bar     ÖstraTorn101 · ANALOGUE MONO SYNTHESIZER · POWER rocker and LED
preset row    display/selector · < > · SAVE · LOAD · MENU · INSPIRED BY SH-101
row 1         LFO · VCO · SOURCE MIXER · VCF · VCA · ENV
row 2         ARPEGGIATOR · SEQUENCER · PERFORMANCE · nameplate · level meter
```

* Brushed-metal body, recessed section panels with engraved titles and screws.
* One fader per continuous control: dark cap with an amber band, scale ticks
  either side, and the value underneath in engineering units (Hz, ms, %, oct, ct)
  computed through the same taper the engine uses.  Double-click restores the
  reference patch.
* Switch-like controls are drop-downs in tinted windows with an orange arrow,
  showing the real positions (`Tri`, `8'`, `-2 Oct Narrow`, `Gate+Trg`, `Up/Down`).
* A level meter fed by the processor's block peaks, with an LED ladder from
  −36 dBFS to 0.
* POWER is a real standby: it mutes the output while the voice keeps running, so
  leaving standby does not click or restart a note.  It is saved with the session.

## Presets

* **Factory**: 43 patches in eight categories, exposed as the plugin's **program
  list**, so a host's own preset menu lists them as well as the panel's selector.
* **User**: one text file per preset under `%APPDATA%\ÖstraTorn101\Presets\*.otp101`
  (SAVE, LOAD, MENU → delete / open folder).  The file format is keyed by
  parameter name, stores normalized values (so a later calibration change cannot
  corrupt old patches), carries the sequencer pattern for sequenced patches, and
  is forward compatible: unknown keys are ignored and missing keys fall back to the
  reference patch.

## Product name: the umlaut lives in the display name

The instrument is called **ÖstraTorn101**. Where the name is *shown* — the panel,
the plugin's own name, the preset library folder — it reads exactly that:

| Where | Name |
| --- | --- |
| Panel title and nameplate | **ÖstraTorn101** |
| `getName()` (what a host's own UI shows) | **ÖstraTorn101** |
| User preset folder | `%APPDATA%\ÖstraTorn101\Presets` |
| VST3 class name (host's plugin *list* entry) | `OstraTorn101` |
| Bundle on disk | `OstraTorn101.vst3` |

The last two are ASCII on purpose, after two attempts at carrying the umlaut all
the way:

1. **A non-ASCII `PRODUCT_NAME` in CMake breaks the plugin outright.** It becomes a
   *compiler command-line define*, and the build tool's Windows argv handling
   mangles it: the VST3 manifest ended up holding invalid UTF-8 (`EF BF 96` where
   `C3 96` belonged) and the host test then found **zero plugins** in the bundle —
   unloadable in any DAW.
2. **A non-ASCII VST3 class name stops hosts instantiating it.** With the class name
   set to the UTF-8 bytes, the DLL loads and exports `GetPluginFactory` (verified
   with `tools/load_dll_check.ps1`, which splits "the DLL fails to load" from "the
   wrapper fails to initialise"), the scan lists the plugin, and then
   `createPluginInstance` fails with "Unable to load VST-3 plug-in file". Reverting
   only that one macro made it load and play again, so the class name stays ASCII
   until someone has time to find out why.

The mechanism for a UTF-8 class name is therefore in place but **currently unused**:

* `JucePlugin_DisplayName="\303\226straTorn101"` — a C escape sequence, so the
  command line is pure ASCII while the compiled string holds the correct UTF-8
  bytes.  `getName()` returns it through `juce::String::fromUTF8` and the panel
  prints it.
* A **local patch in the JUCE checkout** adds `JucePlugin_Vst3ClassName` (default:
  `JucePlugin_Name`) for the class name, so setting that macro in CMakeLists.txt is
  all it takes to try the umlaut again — see "Local patches" below.

Note for anyone reading the code: `juce::String(const char*)` interprets its input
as **Latin-1**, not UTF-8 — only `juce::String::fromUTF8` (or a `StringRef`)
decodes UTF-8.  Passing a UTF-8 literal to a `const juce::String&` parameter is how
the panel first showed "Ã–straTorn101".

## Two more bugs this round found and fixed

1. **Every switch control landed on the wrong position in the plugin.** JUCE's
   *raw* parameter value is the parameter's value in its own range: for a choice
   parameter that is the item index (0…n−1), not the normalized position the
   engine's taper expects.  Feeding it straight to the engine meant, for example,
   "LFO wave = square" (index 1 of 4) arrived as 1.0 and became *noise*.  The
   processor now converts raw → normalized per parameter (a lock-free multiply in
   the audio callback), and the editor test checks switch positions all the way
   through to the parameters the engine is using.
2. **A saved user preset lost its switch positions** for the same reason, which is
   what the preset round-trip test now covers.

Earlier rounds fixed the blank editor window (sizing before the controls existed),
invisible faders (`LookAndFeel_V4` gave a 56-pixel fader a 17-pixel groove) and
read-outs that showed raw 0..1 numbers (`SliderAttachment` installs its own
value→text formatter, so the panel's has to be set after it).

## Rebuilding

```bash
MSYS_NO_PATHCONV=1 cmd /c "C:\Users\bbhal\sh101\tools\build_vst3_msvc.bat"   # vcvars + Ninja: VST3, host test, editor test
```

CMake's "Visual Studio 17 2022" generator cannot use this machine's Build Tools
instance (it is not registered in the VS Installer database, so CMake reports "the
instance is not known to the Visual Studio Installer").  The `.bat` loads
`vcvars64.bat` and drives Ninja instead.

## Why there is no MinGW build

JUCE cannot be built with MinGW/clang, by design:

1. `juce_core/system/juce_TargetPlatform.h` hard-errors on MinGW.
2. The same header sets `JUCE_64BIT` only under `_MSC_VER`, so a MinGW build
   compiles as 32-bit internally and every `pointer_sized_uint` cast fails.
3. The Windows GUI/graphics code uses SDK enumerators mingw-w64 does not ship.

A MinGW experiment (zig clang + libc++, with 1 and 2 patched locally) got
`juceaide` from 24 errors down to 5 before it was stopped as a dead end.
`tools/build_vst3.sh` is kept as that experiment; it is not the supported path.
The uv-installed zig toolchain (`build.sh`) remains what builds and tests the DSP;
it needs no MSVC.

## Local patches to the JUCE checkout

Both are documented in place, in `C:\Users\bbhal\juce-src`:

1. `juce_audio_plugin_client/VST3/juce_VST3ModuleInfo.h` — `JucePlugin_Vst3ClassName`
   (the product name fix above).
2. `juce_core/system/juce_TargetPlatform.h` — the MinGW experiment's policy error
   (not used by the MSVC path).
