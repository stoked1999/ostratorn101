# SH-101 VST Emulation — Agent Engineering Brief

## Mission
Build a monophonic software synthesizer that recreates the signal flow and characteristic behavior of the 1982 Roland SH-101 from its service documentation. Target a modern plugin format (VST3 first; CLAP/AU optional) using C++ and JUCE unless there is a strong reason to choose another framework.

Treat the 1982 service schematic as the primary electrical source. Do not merely build a generic subtractive synth with similar controls. Reproduce the topology block-by-block, then optimize it for real-time DSP.

Do not copy Roland trademarks, logos, panel artwork, or manual text into the distributed product. The engineering target is behavior/sound, not branding.

## Primary source documents

1. Combined SH-101 owner's/service manual scan:
   https://synthfool.com/docs/Roland/SH_Series/Roland%20SH-101%20Owners%20%26%20Service%20Manuals.pdf

   Important service-note pages in the combined PDF are around PDF pages 28–35:
   - Service specifications / physical layout
   - Block diagram and CEM3340 diagram
   - CPU/program notes
   - Adjustment/calibration procedures
   - Jack/bender schematic
   - Main control/synth board layout
   - Full synth-board circuit schematic
   - Parts list

2. Roland official SH-101 technical specifications:
   https://support.roland.com/hc/en-us/articles/201921519-SH-101-Technical-Specifications

3. Roland original owner manual PDF:
   https://cdn.roland.com/assets/media/pdf/SH-101_OM.pdf

4. Useful IR3109 technical analysis/reference:
   https://electricdruid.net/roland-filter-designs-with-the-ir3109-or-as3109/

5. CEM3340 technical/application reference:
   https://www.digisound80.co.uk/digisound/modules/80-2_files/80-2.pdf

6. Repair/component-identification reference:
   https://secretlifeofsynthesizers.com/roland-sh-101/

## Verified original architecture

### Overall signal path
VCO -> source mixer -> VCF -> VCA -> output amplifier.

Modulation/control blocks include a single ADSR envelope, LFO/clock, keyboard CV, portamento, bender/mod grip, noise/random sources, CPU-based keyboard assignment/arpeggiator/sequencer, and CV/gate I/O.

The original is monophonic.

### Main ICs from the service parts list
- TMP80C49P-6-7301 — CPU / keyboard assignment / sequencer / arpeggiator control
- CEM3340 — VCO
- IR3109 — VCF
- BA662A (offset-selected) — VCA
- MB84013B / 4013-family dual D-type flip-flop — used in digital/divider functions including the sub oscillator path
- TC4052BP — dual 4-channel multiplexer
- HD14556B — dual BCD-to-4 decoder
- IR9022 — low-power op amp
- TL062 — low-power BiFET op amp
- M5218L — op amp

Other listed semiconductors include 2SA1015, 2SC1815, 2SC1583, 2SK30A and a selected 2SC945 used in the noise generator.

## VCO

### Hardware basis
The oscillator core is a CEM3340.

### User behavior to implement
- Octave/range: 16', 8', 4', 2'
- Fine tune: approximately +/-50 cents
- Main audio outputs used by the SH-101: sawtooth and pulse/square
- Pulse width control
- PWM source selector: ENV / MANUAL / LFO
- LFO pitch modulation depth

The CEM3340 is capable of simultaneous saw, triangle and pulse outputs, but the SH-101's source mixer exposes the saw and pulse outputs as its principal oscillator audio sources.

### DSP implementation guidance
Start with a band-limited oscillator, not a naive discontinuous saw/pulse. PolyBLEP/minBLEP is acceptable for the first implementation. For higher fidelity, recreate the CEM3340 transfer behavior and output scaling from the application data and the SH-101's surrounding circuit.

Pitch should internally be represented as exponential 1 V/oct-style control. Keep modulation in pitch/CV units until the last conversion to oscillator frequency.

Add optional very small analog-style pitch drift only after the deterministic version is validated. Drift is not a substitute for correct circuit behavior.

## Sub oscillator

The sub oscillator is derived from the VCO using flip-flop/divider logic rather than being an independent free-running oscillator. The schematic/controls expose three modes:
- 1 octave down square
- 2 octaves down square
- 2 octaves down narrow/asymmetric pulse

This phase-locking to the main VCO is important. Implement the sub oscillator by dividing the main oscillator state/edge stream so its phase behavior follows the VCO.

## Noise generator

The service parts list identifies a selected 2SC945 transistor for the noise generator, with analog amplification/conditioning around it (including IR9022 circuitry in the instrument).

For the first DSP version, use white noise with deterministic seeding for tests, then shape/scale it to match measured SH-101 noise spectrum and amplitude. Do not assume ideal flat white noise is the final sound.

## Source mixer

Four source levels feed the filter:
- Pulse/square
- Sawtooth
- Sub oscillator
- Noise

Do not assume that four digital gains summed at unity will reproduce the hardware. Model the actual level scaling/headroom from the schematic. The input level into the IR3109 matters because nonlinear behavior changes with drive.

## VCF — highest-priority authenticity block

### Hardware basis
The filter IC is the Roland IR3109.

The IR3109 contains four OTA-based filter stages plus buffers and exponential cutoff-control circuitry. In the SH-101 it is used as a 4-pole low-pass filter (nominally 24 dB/octave).

### Original controls/spec
- Cutoff: approximately 10 Hz to 20 kHz
- Resonance: zero through self-oscillation
- Envelope depth
- LFO/mod depth
- Keyboard tracking: 0–100%

The service calibration procedure explicitly tunes VCF tracking while the filter is resonating/self-oscillating, which is evidence that resonance behavior and cutoff CV tracking are part of the intended design.

### DSP implementation guidance
Do NOT replace this with a generic biquad cascade and call it finished.

Recommended development path:
1. Create a four-stage OTA-integrator reference model based on the IR3109 topology.
2. Implement the exact SH-101 feedback/resonance network from the service schematic.
3. Include nonlinear saturation/limiting where the schematic and measurements show it.
4. Solve the feedback loop using a zero-delay-feedback / topology-preserving approach if necessary for stable high resonance.
5. Oversample the nonlinear filter path, initially 2x or 4x, and benchmark quality vs CPU.
6. Ensure smooth self-oscillation and musical 1 V/oct-like tracking when key follow is at maximum.

Important: do not copy the resonance implementation from a Juno merely because it also uses an IR3109. The surrounding feedback and gain topology matters.

## VCA

### Hardware basis
BA662A, selected/offset-trimmed version.

### Modes
- ENV: amplitude follows the ADSR
- GATE: fixed level while gate is high

### DSP implementation
First create the correct control law and gain staging. Then add a mild nonlinear VCA model based on measurements/reference behavior. The interaction of filter output level, resonance and the BA662 path can affect perceived character.

## Envelope generator

One ADSR envelope is shared as a control source.

Original specified ranges:
- Attack: ~1.5 ms to 4 s
- Decay: ~2 ms to 10 s
- Sustain: 0–100%
- Release: ~2 ms to 10 s

Trigger modes:
- GATE + TRIG
- GATE
- LFO

The envelope modulates filter cutoff and can be used for pulse-width modulation. It also controls the VCA in ENV mode.

Do not implement the ADSR as arbitrary linear ramps. Inspect the charging/discharging network in the service schematic and match the analog curve shape and control taper. At minimum, use exponential/RC-like segments with parameter mappings fitted to the documented time ranges.

## LFO / modulator

Original rate range: about 0.1 Hz to 30 Hz.

Original modulator selections are triangle, square, random, and noise.

The LFO/modulator can affect VCO pitch and VCF cutoff; the LFO can also be selected as the PWM source. Random and noise modulation are distinct behaviors and should not be collapsed into the same algorithm.

## Portamento

- Time range: approximately 0–5 seconds
- Modes: ON / OFF / AUTO

AUTO should apply glide in the intended legato context rather than always gliding every note.

## MIDI/voice behavior

The hardware uses a TMP80C49-class CPU for keyboard handling, arpeggiator and sequencer, while the audio path is analog.

For a plugin, do not emulate the CPU instruction-by-instruction unless historical digital behavior becomes an explicit target. Reproduce observable behavior instead:
- monophonic note priority/legato behavior from the original
- gate/trigger behavior
- portamento modes
- arpeggiator behavior
- simple step sequencer behavior
- key transpose behavior

The original sequencer supports up to 100 steps according to Roland's specifications.

## CV/gate reference behavior from the original

Useful for understanding the control architecture even if the plugin receives MIDI:
- CV standard: 1 V/octave
- Gate input threshold: +2.5 V or more
- Gate output: approximately 0 V off / 12 V on
- CV input: approximately 0–7 V
- CV output documented around 0.415–5 V over the keyboard/transposition range

Internally, use continuous floating-point control signals; emulate DAC quantization only if testing shows it contributes audibly to a specific function.

## Calibration clues from the service notes

The original service procedure includes separate adjustments for:
- D/A converter tune/width/linearity
- VCO width (octave scaling)
- VCO tune
- range width
- pulse width
- VCF width/tracking
- LFO modulation offset

Use these as a map of which circuit parameters materially affect behavior.

A useful software equivalent is to expose these only as internal calibration constants, not normal user controls. Write automated tests for oscillator octave scaling, filter self-oscillation tracking, PWM center/duty behavior, and envelope timing.

## Suggested software architecture

### DSP objects
- `VoiceController` — mono note stack, priority, legato, gate/trigger
- `Portamento`
- `SH101VCO`
- `SubOscDivider`
- `NoiseGenerator`
- `SourceMixer`
- `SH101VCF`
- `SH101VCA`
- `SH101Envelope`
- `SH101LFO`
- `Arpeggiator`
- `StepSequencer`
- `OutputStage`

### Processing order per sample / small block
1. Process MIDI and mono note state.
2. Generate target pitch CV.
3. Apply portamento.
4. Generate LFO/random/noise control sources.
5. Advance envelope.
6. Convert pitch CV to VCO frequency and generate VCO waveforms.
7. Generate phase-locked sub oscillator.
8. Mix saw/pulse/sub/noise with hardware-inspired gains.
9. Compute filter cutoff CV from manual cutoff + envelope + LFO + keyboard tracking + bender.
10. Process 4-pole IR3109-inspired VCF including feedback/nonlinearity.
11. Apply BA662-inspired VCA using ENV or GATE control.
12. Apply output stage and final safety gain.

## Parameter mapping

Prefer normalized 0..1 plugin parameters, but map them to the original ranges/tapers rather than linearly mapping everything.

At minimum expose:
- LFO rate and waveform
- VCO range, tune, mod depth
- PWM source and amount/manual width
- saw level
- pulse level
- sub level and sub mode
- noise level
- filter cutoff
- resonance
- filter envelope amount
- filter modulation amount
- keyboard tracking
- ADSR
- envelope trigger mode
- VCA mode
- portamento time/mode
- output volume
- arpeggiator controls
- sequencer controls

## Fidelity milestones

### Milestone 1 — functional clone
- Correct mono voice behavior
- Band-limited VCO
- Correct sub divisions
- ADSR/LFO/portamento
- 4-pole resonant filter
- VCA
- VST3 build

### Milestone 2 — circuit-informed model
- Match CEM3340 waveform levels/shape
- Match source-mixer gain staging
- Implement IR3109 OTA stage model
- Implement SH-101-specific resonance feedback
- BA662-inspired VCA nonlinearity
- Hardware-like envelope curves
- Proper control tapers

### Milestone 3 — measured calibration
Use recordings/measurements from a real SH-101 if available:
- isolated saw at several pitches
- isolated pulse at several widths
- sub modes
- noise spectrum
- filter sweeps at 0/25/50/75/100% resonance
- filter self-oscillation frequency vs keyboard pitch
- mixer drive tests at multiple source levels
- envelope transient captures at min/mid/max A/D/R
- VCA response vs envelope voltage

Fit model constants against these measurements. The service manual defines topology and calibration intent, but a schematic alone is not enough to guarantee a perceptually exact clone because real component tolerances and nonlinear transfer curves matter.

## Automated tests

Create tests for:
- oscillator frequency accuracy over MIDI range
- octave switching exactly doubles/halves frequency
- sub oscillator remains phase-locked
- pulse width stays within safe nonzero limits
- no alias blow-ups at high pitch/PWM
- filter stable at maximum resonance
- filter self-oscillation tracks keyboard at full key follow
- envelope times fall inside documented ranges
- legato and retrigger modes behave correctly
- plugin produces no NaN/Inf/denormal failures
- deterministic output in test mode

## Performance targets

- 44.1/48/88.2/96 kHz support
- stable at common host block sizes
- no heap allocations in audio callback
- parameter smoothing for all audio-rate-sensitive controls
- optional 2x/4x oversampling around nonlinear VCF/VCA path
- keep one instance comfortably real-time on a normal modern CPU

## What NOT to do

- Do not use a generic ladder filter; the SH-101 uses an IR3109 OTA cascade.
- Do not make the sub oscillator an independent oscillator.
- Do not use naive saw/pulse generation that aliases heavily.
- Do not treat slider positions as linearly proportional to physical frequency/time without validating the taper.
- Do not copy a Juno filter implementation and assume it is identical.
- Do not spend time emulating the vintage CPU instruction set before the audio path is accurate.
- Do not claim a 1:1 emulation until it has been compared against measured hardware.

## First coding task

Create the project skeleton and a headless DSP test executable before building the GUI.

Implement in this order:
1. Mono note/gate/trigger engine
2. CEM3340-inspired band-limited saw + pulse oscillator
3. Flip-flop-derived sub oscillator
4. Noise and source mixer
5. ADSR and LFO
6. Initial four-pole filter
7. VCA
8. Parameter automation/smoothing
9. VST3 wrapper
10. Replace the initial filter with the full IR3109/SH-101 circuit-informed model
11. Add arpeggiator/sequencer
12. Add UI only after DSP tests pass

For every module, include comments linking the implementation decision to the relevant block/schematic in the service notes and clearly mark anything that is an approximation rather than directly derived from the circuit.

# SONIC FIDELITY FIRST

This project should sound and respond unmistakably like a great Roland SH-101, but it does not need to reproduce every historical limitation when that limitation does not contribute to the sound. The priority is the musical character of the analog voice: oscillator shape and level, sub-oscillator relationship, mixer drive, IR3109 filter behavior, resonance and self-oscillation, envelope curves, BA662-style VCA response, gain staging, modulation depth, and the small nonlinear interactions that create the characteristic punchy and juicy SH-101 sound.

Do not turn the instrument into a generic modern subtractive synth. Modern conveniences are acceptable only when they do not alter the default SH-101 sonic behavior.

## Sonic priorities

Treat these as higher priority than historically exact UI or digital-control limitations:

1. Match the oscillator and sub-oscillator spectrum, phase relationship, pulse-width behavior, and level.
2. Match source-mixer gain staging and the way signal level drives the filter.
3. Model the four-stage IR3109 filter topology, including resonance feedback, cutoff tracking, self-oscillation, level-dependent behavior, and saturation/nonlinearity in the surrounding circuit.
4. Match the envelope timing and curvature, especially the fast attack and decay behavior that gives SH-101 bass and pluck sounds their punch.
5. Match the BA662-style VCA response and output-stage gain behavior.
6. Preserve the original modulation ranges and interactions where they audibly affect pitch, PWM, filter cutoff, resonance, or amplitude.
7. Model realistic analog variation only where it improves authenticity; do not add exaggerated random drift or instability that makes the instrument sound less like a well-calibrated SH-101.

The target is not one exact serial number. The target is the characteristic sound and response of a healthy, well-calibrated original SH-101.

## Modern-host policy

The original SH-101 did not have MIDI, so MIDI and DAW integration are transport/control layers rather than part of the analog voice. They may be modernized as long as the default sound engine remains faithful.

Required behavior:

- MIDI Note On/Off must drive the monophonic note, gate, trigger, priority, legato, and portamento engine correctly.
- Pitch bend should map naturally to the original pitch-bender behavior/range by default.
- MIDI CC and DAW automation may control existing SH-101 parameters.
- MIDI clock and host tempo may sync the arpeggiator/sequencer and optionally the LFO, but free-running original-style timing must remain available.
- Velocity and aftertouch may be offered as optional modern modulation sources, but must be OFF by default and must not change the initialized/original sound.
- Sustain pedal support is acceptable as a host convenience if it is implemented without breaking monophonic gate/legato logic.
- All Notes Off / panic handling should be implemented for host safety.
- Do not add unison, stereo widening, extra oscillators, effects, or additional filter modes to the core emulation unless they are clearly separated as optional extras and cannot alter the default SH-101 patch behavior.

## Fidelity hierarchy

When documentation, idealized circuit theory, and audible/bench measurements disagree, prefer evidence in this order:

1. Measured behavior from one or more healthy, calibrated original SH-101 units.
2. Roland service-manual calibration procedures and schematics.
3. Component datasheets for the exact original parts.
4. Multiple verified recordings/measurements of original units.
5. Circuit simulation.
6. Generic DSP approximations only as a temporary implementation step.

Any approximation must be clearly marked in source comments and test notes so it can later be replaced with a more faithful model.

The acceptance test is primarily audible and measurable sonic behavior, not historical inconvenience. If a modern feature is inaudible when unused, it is acceptable; if it changes the default oscillator/filter/envelope/VCA behavior, it is not.

## Validation requirement

Do not call a module "finished" merely because it sounds similar. Create automated or repeatable validation tests for:

- oscillator frequency and scaling
- waveform shape and pulse-width range
- sub-oscillator phase/division behavior
- mixer gain structure
- filter cutoff tracking
- resonance amount and self-oscillation
- filter FM/envelope depth
- envelope attack/decay/release timing and curves
- VCA response
- LFO rates and waveform timing
- portamento timing and AUTO behavior
- note-priority/legato/gate/trigger behavior
- arp patterns and clocking
- sequencer playback semantics

Where possible, compare measurements against an actual SH-101 at multiple knob positions rather than only at nominal settings.
