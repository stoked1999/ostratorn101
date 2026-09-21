# ÖstraTorn101 — v1.0 roadmap

Status of this document: the agreed scope for the next version. The current
installed build is a working prototype that now also carries the sequencer step
editor and the cymatic display; v1.0 is the release-quality product.

## What is already done

* DSP core modelled from the service documentation, 78 test cases / 3518 checks.
* Panel UI rebuilt from the supplied reference: metal body, section panels,
  faders with amber bands, engineering-unit read-outs, level meter, POWER standby.
* 43 factory presets exposed as host programs; save/load/delete user preset
  library on disk.
* Analogue variation: bounded drift, per-note tolerance, output noise floor.
* Sequencer step editor: 16 slots paged through the 100-step memory — drag a slot
  for pitch, click for a rest, shift/right-click for a tie, REC writes played notes
  in and advances, FOLLOW keeps the page on the running sequence.
* Cymatic display beside the level meter: the standing-wave figure the note makes
  on the plate, drawn on the editor's timer and fed by the audio thread's own
  lock-free block copy.
* VST3 built, installed in both system VST3 folders, verified through a host.

## 1. Sequencer step editor  (built)

The sequencer plays preset melodies and can now be programmed from the panel.
What it does:

* 16 step slots on the panel, paged up to 100 steps, showing each step's note.
* Drag a slot to set pitch; click for rest; shift/right-click for tie.
* Keyboard entry: REC arms step-write, and playing a note writes it to the
  armed step and advances — how the instrument itself records.
* Playhead follows the running sequence; steps persist in the plugin state and
  in user preset files (the file format already carries patterns).
* New host parameters so the controls are automatable and saved by the host:
  `seqLength` (1..100 steps), `seqGate` (gate length), `seqTranspose`
  (−24..+24 semitones).
* Thread safety: the panel edits a lock-free mirror; the audio thread owns the
  engine's sequencer, so edits are published through the existing pattern queue
  and REC writes are drained from the mirror, never through a shared lock.

## 2. Randomizer

Requested as "sick". Aim for *musical* randomness, not noise:

* RANDOMIZE patch: bounded deltas around the current sound, so every press stays
  playable; sections can be locked (e.g. keep the filter, reroll the mixer).
* RANDOMIZE sequence: generates a melody in the loaded preset's spirit — scale
  aware, with density/octave-range/tie probability controls.
* Deterministic for a given seed so a sound can be reproduced and shared
  (same policy as the analogue variation).
* Tests: bounds per parameter, always-audible output, seed reproducibility.

## 3. Animation

Tasteful, hardware-believable motion rather than decoration:

* **Cymatic display (requested): the sound made visible — built; see
  `renders/cymatic_field.png`.**  A water-plate ripple
  figure in the panel's amber, standing in the status block beside the level
  meter: the note being played sets the petal count, brightness sets the rings,
  the envelope drives the glow, and every note strike sends a ripple out through
  the pattern.  It reads a lock-free copy of the rendered block (the audio
  thread only appends) and is rendered on the editor's timer, so it costs the
  audio thread nothing.
* Amber LED and level-meter ballistics on the meter that already exists.
* Playhead sweeping the step editor.
* A slow, tiny glow/vibration on the LFO rate indicator and on active envelopes.
* Optional "valve warm-up": the panel brightens over the first seconds after load.
* Must cost nothing on the audio thread: all animation reads atomics the
  processor already publishes, on the editor's timer.

## 4. Product completeness

* Installer: per-user MSI/Inno Setup style install with no admin prompt, plus the
  current zip-and-copy path as the portable option.
* Manual (PDF + in-panel help): every control, the preset library, the sequencer,
  keyboard shortcuts, and the approximations the model makes.
* Versioning and a changelog; the plugin reports its version to the host.
* Preset import/export through file dialogs (the library already stores plain
  text files, so this is UI work).
* MIDI learn for the main controls; A/B compare; undo for preset loads.
* A resizable/scalable panel (the layout is currently fixed at 1440x646).
* Release checks: fresh-machine install test, live-host test matrix (Ableton
  Live, Reaper, one other), 24-hour soak for leaks, and a CPU budget statement.
* Legal/credits pass: confirm no trademarks or artwork from the original
  instrument anywhere in the product.

## Known constraints to carry into v1.0

* The VST3 **class name is ASCII** (`OstraTorn101`); the umlaut is carried in the
  display name and the panel. A non-ASCII class name stops hosts instantiating
  the plugin (see docs/VST3.md).
* The bundle file name is ASCII for the same family of reasons.
* Parameter count changes are a compatibility event for saved sessions: add
  parameters at the end of the enum and never reorder existing ones.
* Any new audio-thread code stays allocation-free; new state is passed through
  the existing lock-free queue or through atomics.
