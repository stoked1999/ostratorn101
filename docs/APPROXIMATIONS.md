# Approximation register

The brief requires: *"Any approximation must be clearly marked in source comments
and test notes so it can later be replaced with a more faithful model."* This
file is that register. Every entry lists what is approximated, where it lives,
why, and what would replace it.

Markers in the source are `[APPROX]` (comment tag). Status values:

* **structural** — the topology is right, only coefficients need measuring;
* **substitute** — a placeholder algorithm that must be replaced (e.g. polyBLEP
  where a CEM3340-derived model belongs);
* **assumption** — a documented behaviour gap where the sources used do not say.

| # | Item | Where | Status | Replacement path |
| --- | --- | --- | --- | --- |
| 1 | Oscillator band-limiting via polyBLEP | `SH101VCO.cpp`, header comment; alias floors recorded in `docs/VALIDATION.md` and pinned by `tests/test_oscillators.cpp` | substitute | Derive the CEM3340 transfer/output scaling from the application data (brief reference 5) and re-measure with `tools/alias_probe.cpp`; the current numbers (−24 dB worst case at 5 kHz) are the baseline to beat |
| 2 | CEM3340 waveform levels/shape (`cemSawLevel`, `cemPulseLevel`, `cemSquareDuty`) | `Calibration.h` | structural | Measure isolated saw/pulse at several pitches and widths on a real unit, then fit |
| 3 | Analog pitch drift | `SH101VCO` (`setAnalogDrift`), off by default | assumption | The brief asks for drift only after the deterministic model is validated, and warns against exaggerated instability; enable/fit only with measurements |
| 4 | Noise source spectrum and amplitude (`noiseAmp`, `noiseShelfHz`) | `NoiseGenerator.cpp`, `Calibration.h` | substitute | Measure the unit's noise spectrum; replace the single high-pass conditioning with the fitted response |
| 5 | Source-mixer gains and filter-input drive (`saw/pulse/sub/noiseMixGain`, `mixerHeadroom`) | `SourceMixer.cpp`, `Calibration.h` | structural | Mixer drive tests at multiple source levels (brief, milestone 3) |
| 6 | IR3109 stage/feedback coefficients and saturation headroom (`kInputHeadroom`, `kFeedbackHeadroom`, `kStateHeadroom`, `resSelfOscK`) | `SH101VCF.cpp`, `Calibration.h` | structural | Value-by-value transcription from the synth-board schematic plus measured filter sweeps at 0/25/50/75/100% resonance |
| 7 | Resonance make-up / bass loss | `SH101VCF` (no input compensation is applied) | assumption | The IR3109 designs differ in how (and whether) the feedback network compensates DC loss; measure the passband level vs resonance and add the fitted compensation |
| 8 | Self-oscillation frequency vs cutoff (tanh lag) | `SH101VCF`; measured at ≤ 1.2% error in `tests/test_filter.cpp` | structural | Compare self-oscillation frequency vs keyboard pitch at full key follow against a real unit and adjust the feedback/stage constants |
| 9 | BA662A VCA control law and compression (`vcaGainScale`, `vcaNonlinearity`, `vcaOffLeakage`) | `SH101VCA.cpp`, `Calibration.h` | structural | VCA response vs envelope voltage measurement (brief, milestone 3) |
| 10 | Envelope time-constant convention | `Calibration.h` (`envAttackTarget`, `envSettleRatio`), documented in `SH101Envelope.cpp` | assumption | The parameter time is defined as the time to cover 99% of a segment's excursion (attack: charge-to-threshold in exactly the parameterised time). This makes the documented ranges measurable; verify against the unit's measured segment times and adjust the convention if it differs |
| 11 | LFO noise modulator shaping | `SH101LFO.cpp` (one-pole smoothing whose corner scales with the rate) | substitute | The brief requires that random and noise modulation stay distinct (they are), but the noise modulator's exact smoothing is not documented in the sources used; fit from meascurrents if available |
| 12 | PWM polarity/depth | `SH101ENGINE.cpp` step 6 (`pwmDepth = amount * 0.8` around the manual width) | assumption | Measure PWM duty vs envelope/LFO at several manual widths |
| 13 | LFO pitch-modulation depth (`maxPitchModOctaves_ = 1.0`) | `SH101Engine.h` | assumption | Measure the depth at slider maximum on a real unit |
| 14 | DAC quantisation not modelled | brief, "CV/gate reference behaviour" — deliberately continuous control signals | assumption | Only if a test shows the DAC steps are audible (brief: "emulate DAC quantization only if testing shows it contributes audibly") |
| 15 | Arpeggiator gate length (0.5 of the step) and octave ordering | `Arpeggiator.h` | assumption | Compare arp patterns/gate lengths against the unit |
| 16 | Sequencer step gate length, tie semantics, transpose interaction | `StepSequencer.h` | assumption | Compare against the unit; the 100-step limit is from the published specification |
| 17 | Arpeggiator/sequencer clock source | `Arpeggiator`/`StepSequencer` free-running clock by default, host clock opt-in | assumption | The sources used do not state the original clock source; free-running is the safe default, and the host clock is a modern convenience |
| 18 | Pitch bend range (±2 semitones) and bender routing (VCO/VCF/LFO) | `SH101HostAdapter`, `SH101Engine::setPitchBendSemitones` | assumption | Measure the bender's range and switchable destinations; only pitch is currently routed |
| 19 | Output-stage gain and headroom (`outputGain`, `outputHeadroom`, `outputDcBlockHz`) | `Calibration.h`, `OutputStage.cpp` | structural | Fit to the measured unit's output level and output-amplifier behaviour |
| 20 | Oversampling filters | `Oversampler.h` (16 taps per phase, Blackman-windowed sinc, cutoff 0.28 in the 2x domain) | structural | The brief asks to benchmark 2x/4x against CPU; both are implemented and measured. Improve the transition band if a measurement shows audible aliasing |
| 21 | Velocity/aftertouch | `SH101HostAdapter` (`velocityToLevel_`, off by default) | assumption (policy) | The brief allows these as optional modern modulation sources but requires them off by default and not changing the initialised sound — which is what is implemented |
| 22 | Analog drift of the filter/VCA/component tolerances | not modelled beyond the calibration constants | assumption | Model only "where it improves authenticity" (brief), i.e. after hardware measurement |
| 23 | JUCE plugin shell | `src/plugin/juce/` | **unverified** | Never compiled here (no MSVC toolchain/JUCE on this machine). See `docs/VST3.md` |

## What is *not* approximated (implemented from the documented design)

* Monophonic note behaviour: last-note priority, held-note stack, legato, gate
  and trigger semantics.
* The sub oscillator is derived from the VCO's edge stream by a divider counter —
  it is not an independent oscillator, and a test asserts every transition lands
  on a VCO cycle boundary.
* The VCF is a four-stage OTA/integrator cascade with an implicit
  (zero-delay-feedback) resonant loop — not a biquad, not a generic ladder, not a
  Juno feedback network.
* The envelope uses analog RC charge/discharge segments, not linear ramps; a test
  measures the curvature.
* Control ranges and tapers come from the documented ranges (1.5 ms … 4 s attack,
  2 ms … 10 s decay/release, 0.1 … 30 Hz LFO, 0 … 5 s portamento,
  10 Hz … 20 kHz cutoff).
* The service-manual trimmers are exposed as internal calibration constants
  (`Calibration.h`), not as user controls.
* Mono signal path throughout: no unison, stereo widening, effects or extra
  oscillators/filter modes.
