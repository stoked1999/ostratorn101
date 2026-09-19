# SH-101 emulation — project status and build notes

A monophonic software synthesizer that recreates the signal flow and behaviour of
the 1982 Roland SH-101 from its service documentation, built to the engineering
brief in [`docs/engineering-brief.md`](docs/engineering-brief.md) (revision
"SONIC FIDELITY FIRST"; the file's SHA-256 is recorded in that folder's git
history).

Status: **the DSP voice is complete and validated; the VST3 wrapper is written
but has never been compiled** (this machine has no MSVC C++ toolchain — see
[`docs/VST3.md`](docs/VST3.md)).

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
  Oversampler         2x/4x interpolation and decimation around the nonlinear path
  Calibration         software equivalents of the service-manual trimmers
  Params              normalized 0..1 parameters with the original tapers

src/plugin/         host-facing layer
  SH101HostAdapter    MIDI, automation, sustain pedal, panic, block rendering
  juce/               VST3/Standalone shell (UNVERIFIED — see docs/VST3.md)

tests/              57 test cases / 413 checks — the acceptance evidence
tools/              render_cli (WAV), bench_cli (CPU), alias_probe, adapter_probe,
                    verify_renders.py, check_no_alloc.py
docs/               engineering brief, validation report, approximation register
renders/            WAV renders of eight demo patches
```

## Build and run

The development machine has no MSVC C++ toolchain (Visual Studio 2022 is present
without the C++ workload), so the build uses zig's bundled clang + libc++, which
needs no admin rights and no installation:

```bash
uv tool install ziglang cmake ninja     # one-off, user-local
export PATH="$HOME/.local/bin:$PATH"
bash build.sh                           # builds tests + render + bench + alias probe
./build/sh101_tests.exe                 # the validation suite (exit 0 = pass)
./build/sh101_render.exe --list         # demo patches
./build/sh101_render.exe --patch bass --os 2 renders/bass.wav
python tools/verify_renders.py          # checks the rendered WAVs independently
./build/sh101_bench.exe 5               # CPU cost vs real time
python tools/check_no_alloc.py          # static: no allocation in the audio path
```

`CMakeLists.txt` is the canonical build for machines with a normal compiler and
is what the JUCE/VST3 target uses:

```bash
cmake -S . -B build -DCMAKE_BUILD_TYPE=Release
cmake --build build && ctest --test-dir build
```

## Verification summary

* 57 test cases, 413 checks, 0 failures (`build/test_output.txt`).
* Oscillator: pitch accurate to < 0.01% over MIDI 24–108, ranges exactly
  halve/double, ±50 cent tune, pulse width tracked to ±1% duty.
* Sub oscillator: exactly f/2 or f/4, every transition on a VCO cycle boundary,
  deterministic flip-flop reset.
* Filter: −12.0 dB at the corner and −49.4 dB two octaves up (four poles),
  self-oscillation lands within 1.2% of the commanded cutoff at
  100/400/1200/4000 Hz, tracks the keyboard at 1 V/oct with full key follow,
  stable at maximum resonance with and without oversampling.
* Envelope: every documented time (1.5 ms … 4 s attack, 2 ms … 10 s
  decay/release) measured within 1% of its setting, with the analog convex
  attack (0.676 at half the attack time, not 0.5) and exponential discharge.
* LFO: 0.1 / 1 / 5 / 30 Hz measured to within 0.01%; stepped random and
  continuous noise are distinct behaviours.
* Mixer/VCA: gain structure exact, drive stage compresses and bounds, VCA control
  law linear in the envelope with a modelled OTA compression.
* Host layer: MIDI note on/off (including velocity 0), sustain pedal, All Notes
  Off, ±2 semitone bend, normalized automation round-trip, host-clock sync,
  mono-consistent multi-channel output, and note events inside one block.
* Engine: bit-identical renders for identical seeds, silence without a gate,
  finite and bounded output with everything at maximum at 44.1/48/88.2/96 kHz and
  1x/2x/4x oversampling, block-size independence, **0 bytes allocated during 32
  render blocks**, no safety reset ever triggered.
* CPU: 19.7–108x real time (0.9–5.5% of one core) depending on rate and
  oversampling, measured by `tools/bench_cli.cpp`.

Details, per-test numbers and the outstanding hardware comparisons:
[`docs/VALIDATION.md`](docs/VALIDATION.md).

## Fidelity milestones

| Milestone | State |
| --- | --- |
| 1 — functional clone (mono voice, band-limited VCO, sub divisions, ADSR/LFO/portamento, 4-pole resonant filter, VCA) | **done and tested** (VST3 build outstanding) |
| 2 — circuit-informed model (CEM3340 waveform levels, mixer gain staging, IR3109 OTA stage model and SH-101 feedback, BA662 VCA nonlinearity, analog envelope curves, control tapers) | topology, curves and tapers are in place; **the coefficients that need measured hardware are not fitted yet** — see the approximation register |
| 3 — measured calibration (fit constants to recordings of a real unit) | **not started**: requires an SH-101 or a measurement set. `Calibration.h` exposes exactly the trimmers to fit, and `tools/` provides the measurement harnesses |

## Deliberate non-goals

Per the brief: no branding/trademark use, no polyphony, no unison, no effects, no
stereo widening, no extra oscillators or filter modes, no CPU instruction-set
emulation. Anything modern (MIDI, automation, sustain pedal, host clock) is a
transport/control layer that leaves the default voice untouched.
