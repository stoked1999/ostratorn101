# ÖstraTorn101 — analogue mono synthesizer (VST3)

A monophonic software synthesizer that recreates the signal flow and behaviour of
the 1982 Roland SH-101 from its service documentation, built to the engineering
brief in [`docs/engineering-brief.md`](docs/engineering-brief.md) (revision
"SONIC FIDELITY FIRST").  The panel credits the instrument it is inspired by and
reproduces no manufacturer branding.

Status: **working prototype — complete, built, installed and verified.** This is
the release-quality starting point: [`docs/ROADMAP.md`](docs/ROADMAP.md) tracks
what is finished and what is still open (sequencer step editor, randomizer,
animation, product completeness).

* The DSP voice is validated by the engine test suite: **78 cases / 3518 checks**,
  plus a preset-bank suite and a preset-file suite.
* The VST3 is built with MSVC + JUCE 8.0.15, installed in Live's default VST3
  folder, and verified through JUCE's own hosting classes
  (`tools/vst3_host_test.cpp`): it scans as an instrument, reports its 34
  parameters, accepts MIDI, produces audio and creates its editor.
* The panel is verified by `tools/editor_test.cpp`, which builds the plugin's own
  editor, snapshots it to `renders/plugin_ui.png` and checks that it lays its
  controls out, paints, and round-trips a user preset through the library.
* 43 factory presets, exposed both in the panel and as the plugin's host programs,
  and a user preset library that saves patches as files.

## What exists

```
src/sh101/          the voice — one class per block, named after the brief's object list
  SH101VCO            CEM3340-inspired band-limited saw/pulse/core-square
  SubOscDivider       flip-flop divider sub oscillator (phase-locked to the VCO)
  NoiseGenerator      2SC945-style noise source + conditioning
  SourceMixer         four-source mixer with hardware-inspired gain staging + drive
  SH101VCF            four-stage OTA (IR3109) low-pass, ZDF feedback, oversampled
  SH101VCA            BA662A-style VCA (ENV / GATE modes)
  SH101Envelope       analog-curve ADSR, GATE+TRIG / GATE / LFO trigger modes
  SH101LFO            triangle / square / random (stepped) / noise (continuous)
  VoiceController     mono note stack: priority, legato, ordered gate events
  Portamento          RC glide, ON / OFF / AUTO
  Arpeggiator          up / down / up-down, octave range, internal or host clock
  StepSequencer        100 steps, rests, ties, transpose, internal or host clock
  OutputStage         output amplifier: DC block, level, soft limiting
  SH101Engine         the whole voice, in the brief's documented processing order
  AnalogVariation     thermal drift, per-note tolerance, RC tolerance, output hiss
  Oversampler         2x/4x interpolation and decimation around the nonlinear path
  Calibration         software equivalents of the service-manual trimmers
  Params              normalized 0..1 parameters with the original tapers
  Presets             43-patch factory bank (data + sequencer patterns)
  PresetIO            the preset file format (save / load, no framework needed)

src/plugin/         host-facing layer
  SH101HostAdapter    MIDI, automation, sustain pedal, panic, block rendering
  juce/               VST3/Standalone shell: processor, panel editor, look-and-feel

tests/              78 test cases / 3518 checks — the acceptance evidence
tools/              render_cli (WAV, incl. --preset), bench_cli (CPU), alias_probe,
                    adapter_probe, editor_test (panel + preset library smoke test),
                    vst3_host_test, verify_renders.py, check_no_alloc.py
docs/               engineering brief, validation report, approximation register
renders/            WAV renders of eight demo patches + renders/presets/ (43 patches)
                    + renders/plugin_ui.png (the panel, drawn by the editor test)
```

## The panel

The instrument is drawn as hardware: a brushed-aluminium body with recessed
section panels, chunky faders with amber caps, drop-downs showing real switch
positions, indicator LEDs, panel screws, a nameplate and a level meter.

```
row 1:  LFO · VCO · SOURCE MIXER · VCF · VCA · ENV
row 2:  ARPEGGIATOR · SEQUENCER · PERFORMANCE ·  nameplate and level meter
```

Every fader shows the value it currently holds in engineering units (Hz, ms, %,
oct, ct) computed through the same taper the engine uses, so the panel reads like
the instrument.  Double-clicking a fader returns it to the reference patch.

## Presets

`PRESET` row: the display doubles as the selector, `<` and `>` step through the
whole library, and **SAVE / LOAD / MENU** manage the user library.

* **Factory bank** — 43 patches in eight categories (Bass, Lead, Pad, Pluck,
  Perc, FX, Arp, Seq), also exposed as the plugin's host programs, so a host's own
  preset menu lists them too.  Sequenced patches carry their pattern with them.
* **User presets** — one text file per preset under
  `%APPDATA%\ÖstraTorn101\Presets\*.otp101`, saved and loaded from the panel.
  The format is human-readable, keyed by parameter name, and forward compatible
  (unknown keys are ignored, missing keys fall back to the reference patch).

## Build and run

```bash
# The DSP engine, its tests and the headless renderer (no SDK needed — any C++20 compiler)
bash build.sh && ./build/sh101_tests
./build/sh101_render --list-presets
./build/sh101_render --preset "Acid Bass" renders/acid.wav

# The plugin (needs MSVC + a JUCE 8.x checkout)
tools/build_vst3_msvc.bat
```

See **[docs/BUILDING.md](docs/BUILDING.md)** for both paths in full — requirements
per platform, what `build.sh` produces, how to run the host/editor verification
binaries, and where to start reading the code.

## Fidelity milestones

| Milestone | State |
| --- | --- |
| 1 — topology-faithful model (signal path, oscillator types, four-pole filter, envelope, modulation) | **done**: see `docs/VALIDATION.md` for the measured numbers |
| 2 — circuit-informed model (CEM3340 waveform levels, mixer gain staging, IR3109 OTA model, BA662 VCA nonlinearity, analog envelope curves, control tapers, analog variation) | topology, curves, tapers and tolerance figures are in place; **the coefficients that need measured hardware are not fitted yet** — see the approximation register |
| 3 — measured calibration (fit constants to recordings of a real unit) | **not started**: requires an SH-101 or a measurement set. `Calibration.h` exposes exactly the trimmers to fit, and `tools/` provides the measurement harnesses |
