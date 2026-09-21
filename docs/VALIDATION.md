# Validation report

Every number below is produced by `build/sh101_tests.exe` (`build/test_output.txt`
holds a full run).  Tolerances are acceptance thresholds for the modelled
behaviour; where a comparison against real hardware is still outstanding, that is
stated explicitly rather than implied by a tight tolerance.

## Update — 2026-09-21 (step editor, cymatic display, arpeggiator release)

The suite stands at **82 cases / 3723 checks, all passing**.  New evidence:

| Added check | What it asserts |
| --- | --- |
| `arp_stops_when_the_last_key_is_released` | the voice stops for a key release swept across every point of an arp step (16 offsets, guarded against a vacuous pass) — the case that used to leave the note sounding |
| `switching_a_note_source_off_releases_a_gated_note` | switching ARP or SEQ off while its gate is high releases the note |
| editor test — step editor | REC writes a played note into the armed step, the write head advances, and the slots paint |
| editor test — cymatic display | the figure lights with the sound, reads the played note's pitch, takes its petal count from the note, registers the strike, and draws in the panel's amber (7,763 amber pixels) |
| `seq_tempo_sets_the_step_interval` | BPM × division gives the step rate (120 BPM at 1/16 = 8 steps/s, measured as a 6000-sample interval), the Host clock follows the host's tempo and falls back to the panel's when no host tempo is reported, and the three tempo parameters are appended and named |

The cymatic figure is drawn on the editor's timer from the same lock-free block
copy the audio thread already publishes, so it costs the audio path nothing
(`tools/check_no_alloc.py` covers the audio-thread allocation rule as before).

## How to reproduce

```bash
bash build.sh && ./build/sh101_tests.exe          # 82 cases, 3723 checks, exit 0 = pass
./build/sh101_tests.exe filter                    # substring filter for one area
./build/sh101_tests.exe preset                    # the preset bank alone (7 cases)
./build/sh101_bench.exe 5                         # CPU
./build/alias_probe.exe 44100                     # oscillator alias table
./build/adapter_probe.exe                         # host-layer / gate-event trace
./build/sh101_render.exe --list-presets           # the preset bank
./build/sh101_render.exe --preset "Acid Bass" renders/acid.wav
python tools/verify_renders.py                    # rendered WAV inspection
python tools/check_no_alloc.py                    # static no-allocation check

# Panel UI + preset plumbing (needs the JUCE build; writes the PNG it checks)
MSYS_NO_PATHCONV=1 cmd /c "C:\\Users\\bbhal\\sh101\\tools\\build_vst3_msvc.bat"
./build-plugin-msvc/src/plugin/juce/sh101_editor_test_artefacts/Release/sh101_editor_test.exe renders/plugin_ui.png
"./build-plugin-msvc/sh101_host_test_artefacts/Release/sh101_host_test.exe "C:/Program Files/Common Files/VST3/OstraTorn101.vst3"
```

## The brief's validation list, mapped to tests

| Required validation | Test(s) | Measured result |
| --- | --- | --- |
| oscillator frequency and scaling | `vco_frequency_accuracy_over_midi_range`, `vco_octave_switching_doubles_or_halves`, `vco_fine_tune_range_is_about_50_cents` | MIDI 60: 261.6243 Hz vs 261.6256 expected; MIDI 108: 4186.09 vs 4186.01; ranges 16'/8'/4'/2' = 130.812 / 261.624 / 523.243 / 1046.529 Hz |
| waveform shape and pulse-width range | `vco_pulse_width_range_and_safety` | duty follows the setting to ±1%; requests outside the safe band clamp to 3%…97% |
| sub-oscillator phase/division behaviour | `sub_osc_frequency_is_exact_division_of_vco`, `sub_osc_edges_are_phase_locked_to_vco`, `sub_osc_reset_is_deterministic` | f/2 and f/4 within 0.5%; 100% of sub transitions land on a VCO cycle boundary; identical output across resets |
| mixer gain structure | `mixer_gain_structure_matches_calibration`, `mixer_drive_stage_compresses_and_bounds` | per-source gains exact to 1e-12; the drive stage compresses at high level and stays linear for small signals |
| filter cutoff tracking | `filter_is_four_pole_with_cutoff_tracking`, `filter_self_oscillation_tracks_cutoff` | −12.0 dB at the corner, −49.4 dB at 4×corner; self-oscillation at 101.1 / 400.0 / 1199.9 / 3999.8 Hz for 100 / 400 / 1200 / 4000 Hz commanded |
| resonance amount and self-oscillation | `filter_resonance_is_monotonic_and_stable_at_maximum`, `filter_self_osc_tracks_keyboard_at_full_key_follow` | corner peak rises monotonically with resonance; bounded and finite at maximum with 1x/2x/4x oversampling; key follow at 1 V/oct: 100.0 / 200.0 / 400.0 Hz for MIDI 48 / 60 / 72 |
| filter FM / envelope depth | `filter_cutoff_cv_depth_from_envelope_and_lfo` | +3 octaves of envelope lands on 500→4000 Hz; ±2 octaves of LFO sweeps 125.0 … 2000.8 Hz |
| envelope attack/decay/release timing and curves | `envelope_attack_times_match_documented_range`, `envelope_decay_and_release_times`, `envelope_curves_are_exponential_not_linear`, `envelope_sustain_level_and_parameter_ranges` | attack 0.0015/0.01/0.1/1/4 s measured 0.0015/0.0100/0.0998/0.9977/3.9909 s; decay and release within 1% across 2 ms … 5 s; attack reaches 0.676 at half its time (linear would be 0.500); decay is at 0.101 at half its time |
| VCA response | `vca_env_and_gate_modes`, `vca_nonlinearity_shape` | gain linear in the envelope to 1e-12, closed-VCA leakage < 1e-3 (−80 dB), compression at large signal, exactly linear when the OTA nonlinearity is set to 0 |
| LFO rates and waveform timing | `lfo_rate_accuracy_across_documented_range`, `lfo_waveform_timing`, `lfo_random_is_stepped_and_noise_is_continuous` | 0.1 / 1 / 5 / 30 Hz measured 0.1000 / 1.0000 / 5.0000 / 30.0019 Hz; square duty 0.500; random changes exactly once per cycle (20 in 10 s at 2 Hz) vs 9999 changes for noise |
| portamento timing and AUTO behaviour | `portamento_timing_and_modes`, `engine_portamento_on_and_auto_behaviour` | 0.2 / 1.0 / 3.0 s parameters measured 0.200 / 1.000 / 3.000 s to 99%; ON at 0.5 s settles in 0.500 s; AUTO jumps on a detached note change and glides on a legato one |
| note-priority/legato/gate/trigger behaviour | `voice_last_note_priority_and_stack_return`, `voice_gate_trigger_and_legato_flags`, `voice_sustain_pedal_holds_notes`, `voice_all_notes_off_is_a_panic`, `engine_trigger_modes_control_envelope_restart`, `engine_gate_edges_between_samples_are_not_lost` | last-note priority with correct return to a held note; GATE+TRIG restarts the envelope on a legato note (0.066 → 0.954) while GATE does not (0.066 → 0.060); note events between rendered samples are honoured |
| arp patterns and clocking | `arp_patterns_up_down_updown`, `arp_octave_range`, `arp_gate_length`, `arp_host_clock_sync_is_optional` | up 60 64 67, down 67 64 60, up-down 60 64 67 64…; two octaves 48 52 60 64; gate duty exactly 0.500; host clock advances one step per tick and free-running remains the default |
| sequencer playback semantics | `seq_playback_semantics`, `seq_capacity_and_clocking` | steps 36 43 48 50; per-step gate fractions 0.50 / 0.00 / 0.50 / 1.00 (rest and tie behave); 100-step capacity; transpose +12 shifts playback; external clock steps on ticks only |

Additional coverage (engine and host layer):

| Area | Test(s) | Measured result |
| --- | --- | --- |
| deterministic render | `engine_render_is_deterministic` | bit-identical buffers for equal seeds; different seed changes the output |
| silence without a gate | `engine_is_silent_without_a_gate` | peak < 1e-3 |
| note on/off | `engine_note_on_and_off_envelope_behaviour` | RMS 0.078 with a note, 0.000011 after the release |
| rates, oversampling, extremes | `engine_host_rates_oversampling_and_extremes`, `engine_long_run_stability` | 44.1/48/88.2/96 kHz × 1x/2x/4x with resonance 1.0, all sources, 30 Hz noise LFO, MIDI 96/108: finite, peak < 2.5, 0 safety resets, worst peak over 24 notes 0.854 |
| block-size independence | `engine_block_size_independence` | a 1024-sample block is bit-identical to 1024 single-sample renders |
| parameter smoothing | `engine_parameter_smoothing_has_no_steps` | max sample step 0.0118 at volume 0.2, 0.0387 at 1.0, 0.0110 *during* the change (no discontinuity) |
| no heap allocation | `engine_render_does_not_allocate`, `engine_dsp_types_are_heap_free_value_types`, `tools/check_no_alloc.py` | 0 bytes allocated during 32 blocks; every DSP class is trivially copyable; static scan clean |
| pitch bend | `engine_pitch_bend_maps_to_semitones`, `adapter_pitch_bend_range` | ±2 semitones at the default range, ratio exact to 0.1%; configurable range honoured |
| host MIDI/automation | `test_host_adapter.cpp` (8 cases) | note on/off, velocity 0 = off, sustain pedal, All Notes Off, CC 1/5, normalized round-trip, mono multi-channel output, notes inside one block |

## Oscillator alias performance

`tools/alias_probe.cpp` scans every folded harmonic (not one probe bin) and
compares the band-limited model against a deliberately naive oscillator:

| f0 | pulse width | model worst alias | naive reference worst alias |
| --- | --- | --- | --- |
| 250 Hz | 50% / 25% / 10% | −47.0 / −44.3 / −38.6 dB | +109…+113 dB |
| 500 Hz | 50% / 25% / 10% | −41.3 / −38.9 / −31.1 dB | +15.8 / +23.0 / +31.2 dB |
| 1000 Hz | 50% / 25% / 10% | −35.8 / −35.8 / −27.3 dB | +21.4 / +28.4 / +35.7 dB |
| 4000 Hz | 50% / 25% / 10% | −24.8 / −21.8 / −15.0 dB | +33.4 / +30.9 / +31.8 dB |
| 5000 Hz | 50% / 25% / 10% | −24.0 / −24.0 / −13.8 dB | +33.4 / +30.9 / +31.8 dB |

polyBLEP is 40…65 dB better than naive, and its worst case at high pitch is about
−24 dB (a 10% pulse at 5 kHz is only ~0.9 samples wide per period, which no
edge-BLEP method resolves). These are the numbers a CEM3340-derived oscillator is
expected to improve on.

## CPU cost

`tools/bench_cli.cpp`, 5 s of audio per measurement, one voice, two notes held:

| rate | 1x | 2x | 4x |
| --- | --- | --- | --- |
| 44.1 kHz | 88–109x RT (0.9–1.1%) | 65–77x RT (1.3–1.5%) | 38–43x RT (2.3–2.6%) |
| 48 kHz | 80–100x RT (1.0–1.3%) | 56–70x RT (1.4–1.8%) | 34–38x RT (2.7–2.9%) |
| 88.2 kHz | 44–54x RT (1.8–2.3%) | 33–38x RT (2.6–3.0%) | 19–21x RT (4.7–5.2%) |
| 96 kHz | 43–51x RT (2.0–2.4%) | 31–35x RT (2.8–3.3%) | 18–20x RT (5.1–5.5%) |

Range shown: worst-case patch (max resonance, all four sources, deep modulation)
to plain patch. Well inside the brief's "comfortably real-time" target.

## Rendered audio

Eight demo patches are rendered by `sh101_render` and checked by
`tools/verify_renders.py` (independent stdlib WAV reader):

| patch | peak | RMS | dBFS | spectral centroid |
| --- | --- | --- | --- | --- |
| init | 0.462 | 0.2183 | −13.2 | 1313 Hz |
| bass | 0.523 | 0.0725 | −22.8 | 132 Hz |
| pluck | 0.403 | 0.0366 | −28.7 | 2347 Hz |
| selfosc | 0.708 | 0.4535 | −6.9 | 424 Hz |
| pwm | 0.612 | 0.2282 | −12.8 | 1671 Hz |
| sub | 0.469 | 0.1331 | −17.5 | 156 Hz |
| arp | 0.412 | 0.0690 | −23.2 | 1418 Hz |
| sequencer | 0.401 | 0.0579 | −24.7 | 796 Hz |

## Preset bank (21 patches)

`src/sh101/Presets.h` holds the bank; `build/sh101_tests.exe preset` validates it
(7 cases, 1196 checks). A preset is a claim — "this will sound like X" — so what
is checked is what a test can check:

| Check | Result |
| --- | --- |
| bank well formed (names, categories from the panel's own vocabulary, unique names) | 21 patches, 9 categories |
| every control inside its documented range | `getNormalized` in 0..1 for all 34 controls × 21 patches |
| survives the host round-trip (engine value → 0..1 parameter → engine value) | exact for every control; a drift would be reported by name |
| differs from the reference patch | every preset changes ≥ 4 controls |
| produces sound | all 21 audible, finite, peak 0.09…0.70 (no silence, no clipping) |
| switch controls sit on real positions and have names | 10 switch controls, all positions named |
| sequenced presets carry a pattern | 2 patched sequences, 8 steps each, repeated notes rejected |

Every patch is also rendered to `renders/presets/` (`sh101_render --preset N`) so
the sounds can be auditioned without a DAW. Musical quality is not something a
test can assert; the bank is named after the sounds the instrument is known for
(acid bass, PWM strings, self-oscillation, sequencer lines) and every value is a
documented control position on this model — not a copy of anyone's patch sheet.

## Editor / preset smoke test (JUCE build)

`tools/editor_test.cpp` builds the plugin's own editor (VST3 hosting cannot see
into it) and snapshots it to `renders/plugin_ui.png`. 16 checks, all passing:

| Check | Measured |
| --- | --- |
| preset bank exposed as host programs | 21 programs, e.g. `Bass: Acid Bass` |
| loading a program moves every control | 0 mismatches over 34 controls |
| a sequenced preset programs the engine | 8 steps, notes 36 36 48 39 36 51 48 43 |
| a loaded preset makes sound | peak 0.3831 |
| editor lays out and paints | 908×576, 76 children, 0 with no size, 77.7% of the window painted, amber accents in 4/4 bands |
| read-outs are in engineering units | `5.00Hz`, not `0.6858647` |

Two real bugs were found by this test and fixed: the editor sized itself before
its controls existed (blank window), and `LookAndFeel_V4`'s slider layout left a
56-pixel fader a 17-pixel groove, so the custom fader drawing returned early
(invisible faders). Both are described in [VST3.md](VST3.md).

## Outstanding (needs hardware or a measurement set)

These are the parts of the brief's validation list that **cannot** be closed from
documentation alone. They are the milestone-3 work, and `Calibration.h` names the
exact constants each one would move.

1. CEM3340 waveform levels and shape vs a real unit (saw/pulse amplitude,
   pulse-width law, square duty).
2. Source-mixer gain staging and the drive into the IR3109 (measured sweeps at
   several source levels, compared against the current soft-clip headroom).
3. IR3109 stage/feedback coefficients and its saturation curves (filter sweeps at
   0/25/50/75/100% resonance; self-oscillation frequency vs pitch at full key
   follow; level-dependent behaviour).
4. BA662 VCA transfer curve vs envelope voltage (the current law is linear in the
   control voltage with a modelled compression).
5. LFO waveform exactness (triangle symmetry and start phase) and the LFO trigger
   mode's interaction with the gate.
6. Noise spectrum and amplitude vs a real unit's noise source.
7. Level calibration: output-stage gain vs measured unit output (the current
   constants put a standard patch near −6 dBFS at volume 0.8).
8. Pitch bend range (assumed ±2 semitones) and the bender's VCF/LFO routing.
