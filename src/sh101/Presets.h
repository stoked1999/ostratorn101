// Preset bank: named SH-101 patches.
//
// These are *our* recreations of the sounds the SH-101 is known for, written
// directly against the modelled controls.  They are not, and must not be
// presented as, copies of any factory patch sheet or of a commercial patch
// bank: every value below is a control position on this emulation, chosen to
// land the classic bass / lead / string / pluck / FX / sequencer character.
//
// Two rules make the bank trustworthy:
//   1. A preset only touches controls that exist on the instrument (no invented
//      pitch envelope, no effects) — the sonic range is achieved the same way
//      the hardware does it: source mix, pulse width + PWM, filter, envelope,
//      LFO, and the built-in arpeggiator/sequencer.
//   2. Every preset is validated by the test suite: it must be in range, must
//      survive the normalized round-trip that the plugin uses, must differ from
//      the default patch, and must actually produce sound.
//
// Preset 0 is the model's reference (init) patch, so the bank doubles as a
// starting point for the editor's preset selector.
#pragma once

#include <cstring>

#include "sh101/Params.h"
#include "sh101/StepSequencer.h"

namespace sh101 {

// Applying a preset starts from the reference patch, so an apply function only
// has to state what it changes from it — except for the source mixer levels,
// which are always set explicitly (otherwise a preset that means "saw only"
// would inherit the reference patch's pulse oscillator).
//
// A preset may also carry a sequencer pattern.  The pattern is not a plugin
// parameter (the hardware keeps its sequence in memory, not on the panel), so
// sequenced presets supply `applySequence` and the host/editor programs the
// sequencer when the preset is loaded.
struct PresetInfo {
    const char* category;      // "Init", "Bass", "Lead", ...
    const char* name;
    const char* description;
    void (*apply)(SH101Params&);
    void (*applySequence)(StepSequencer&) = nullptr;   // null = leave the sequence alone
};

// ---- Init -------------------------------------------------------------------
inline void presetInit(SH101Params& p) {
    (void) p;   // the reference patch: pulse oscillator, filter open, no env amount
}

// ---- Bass -------------------------------------------------------------------
inline void presetSubBass(SH101Params& p) {
    p.sawLevel = 0.15; p.pulseLevel = 0.0; p.subLevel = 1.0; p.subMode = 1;
    p.noiseLevel = 0.0;
    p.cutoff = 220.0; p.resonance = 0.12; p.filterEnvAmt = 1.1; p.keyTrack = 0.35;
    p.attack = 0.004; p.decay = 0.5; p.sustain = 0.55; p.release = 0.18;
    p.portamentoMode = 0;
}

inline void presetAcidBass(SH101Params& p) {
    p.sawLevel = 1.0; p.pulseLevel = 0.0; p.subLevel = 0.1; p.subMode = 0;
    p.cutoff = 180.0; p.resonance = 0.82; p.filterEnvAmt = 3.4; p.keyTrack = 0.5;
    p.envTrigger = 0;   // gate + trigger: every note retriggers the filter sweep
    p.attack = 0.002; p.decay = 0.32; p.sustain = 0.06; p.release = 0.12;
    p.vcaMode = 0;
    p.portamentoMode = 0;
}

inline void presetRubberBass(SH101Params& p) {
    p.pulseLevel = 1.0; p.sawLevel = 0.0; p.subLevel = 0.45; p.subMode = 0;
    p.pulseWidth = 0.30; p.pwmSource = 1; p.pwmAmount = 0.0;
    p.cutoff = 420.0; p.resonance = 0.45; p.filterEnvAmt = 2.2; p.keyTrack = 0.4;
    p.attack = 0.003; p.decay = 0.28; p.sustain = 0.25; p.release = 0.12;
    p.portamentoMode = 0;
}

inline void presetElectroBass(SH101Params& p) {
    p.sawLevel = 1.0; p.pulseLevel = 0.0; p.subLevel = 0.6; p.subMode = 0;
    p.cutoff = 520.0; p.resonance = 0.2; p.filterEnvAmt = 2.6; p.keyTrack = 0.3;
    p.envTrigger = 0;
    p.attack = 0.002; p.decay = 0.16; p.sustain = 0.0; p.release = 0.08;
    p.portamentoMode = 0;
}

inline void presetChicagoBass(SH101Params& p) {
    p.pulseLevel = 1.0; p.sawLevel = 0.0; p.subLevel = 0.3; p.subMode = 0;
    p.pulseWidth = 0.45;
    p.cutoff = 340.0; p.resonance = 0.38; p.filterEnvAmt = 1.8; p.keyTrack = 0.45;
    p.filterModAmt = 0.4; p.lfoRate = 0.35; p.lfoWave = 1;   // slow square wobble
    p.attack = 0.004; p.decay = 0.45; p.sustain = 0.45; p.release = 0.2;
    p.portamentoMode = 0;
}

// ---- Lead -------------------------------------------------------------------
inline void presetClassicLead(SH101Params& p) {
    p.sawLevel = 1.0; p.pulseLevel = 0.0; p.subLevel = 0.25; p.subMode = 0;
    p.cutoff = 2200.0; p.resonance = 0.3; p.filterEnvAmt = 1.4; p.keyTrack = 0.7;
    p.attack = 0.012; p.decay = 0.6; p.sustain = 0.8; p.release = 0.3;
    p.portamentoTime = 0.06; p.portamentoMode = 1;
}

inline void presetSquelchLead(SH101Params& p) {
    p.sawLevel = 1.0; p.pulseLevel = 0.0; p.subLevel = 0.0;
    p.cutoff = 700.0; p.resonance = 0.72; p.filterEnvAmt = 2.6; p.keyTrack = 0.6;
    p.attack = 0.006; p.decay = 0.28; p.sustain = 0.35; p.release = 0.18;
    p.portamentoMode = 1; p.portamentoTime = 0.04;
}

inline void presetPwmLead(SH101Params& p) {
    p.pulseLevel = 1.0; p.sawLevel = 0.0; p.subLevel = 0.0;
    p.pulseWidth = 0.5; p.pwmSource = 2; p.pwmAmount = 0.65;
    p.lfoRate = 3.2; p.lfoWave = 0;
    p.cutoff = 3000.0; p.resonance = 0.18; p.keyTrack = 0.6;
    p.attack = 0.01; p.decay = 0.5; p.sustain = 0.8; p.release = 0.25;
    p.portamentoMode = 0;
}

inline void presetWhistleLead(SH101Params& p) {
    p.pulseLevel = 1.0; p.sawLevel = 0.0; p.subLevel = 0.0;
    p.pulseWidth = 0.1; p.pwmSource = 1;
    p.vcoRange = 2;   // 4' — the thin, high whistle register
    p.cutoff = 4200.0; p.resonance = 0.42; p.keyTrack = 1.0;
    p.attack = 0.02; p.decay = 0.4; p.sustain = 0.85; p.release = 0.3;
    p.volume = 0.7;
    p.portamentoMode = 0;
}

// ---- Pad / strings ----------------------------------------------------------
inline void presetPwmStrings(SH101Params& p) {
    p.pulseLevel = 1.0; p.sawLevel = 0.35; p.subLevel = 0.2; p.subMode = 0;
    p.pulseWidth = 0.5; p.pwmSource = 2; p.pwmAmount = 0.8;
    p.lfoRate = 0.45; p.lfoWave = 0;
    p.cutoff = 1900.0; p.resonance = 0.12; p.filterEnvAmt = 0.8; p.keyTrack = 0.35;
    p.attack = 0.45; p.decay = 1.2; p.sustain = 0.9; p.release = 0.9;
    p.volume = 0.75;
    p.portamentoMode = 0;
}

inline void presetSoftPad(SH101Params& p) {
    p.sawLevel = 0.8; p.pulseLevel = 0.0; p.subLevel = 0.35; p.subMode = 0;
    p.cutoff = 900.0; p.resonance = 0.1; p.filterEnvAmt = 0.6;
    p.filterModAmt = 0.5; p.lfoRate = 0.28; p.lfoWave = 0;
    p.attack = 0.9; p.decay = 1.5; p.sustain = 0.9; p.release = 1.3;
    p.volume = 0.7;
    p.portamentoMode = 0;
}

inline void presetVintageStrings(SH101Params& p) {
    p.pulseLevel = 1.0; p.sawLevel = 0.55; p.subLevel = 0.0;
    p.pulseWidth = 0.5; p.pwmSource = 2; p.pwmAmount = 0.7; p.lfoRate = 0.6;
    p.cutoff = 1400.0; p.resonance = 0.2; p.filterEnvAmt = 0.7; p.keyTrack = 0.4;
    p.attack = 0.3; p.decay = 1.0; p.sustain = 0.85; p.release = 0.8;
    p.portamentoMode = 0;
}

// ---- Pluck / percussive -----------------------------------------------------
inline void presetSynthPluck(SH101Params& p) {
    p.sawLevel = 1.0; p.pulseLevel = 0.0; p.subLevel = 0.0;
    p.cutoff = 620.0; p.resonance = 0.5; p.filterEnvAmt = 3.2; p.keyTrack = 0.45;
    p.envTrigger = 1;   // gate: one filter sweep per key press, no retrigger
    p.attack = 0.0015; p.decay = 0.16; p.sustain = 0.0; p.release = 0.09;
    p.portamentoMode = 0;
}

inline void presetSequenceBlip(SH101Params& p) {
    p.pulseLevel = 1.0; p.sawLevel = 0.0; p.subLevel = 0.25; p.subMode = 0;
    p.pulseWidth = 0.22;
    p.cutoff = 1300.0; p.resonance = 0.6; p.filterEnvAmt = 2.2; p.keyTrack = 0.5;
    p.envTrigger = 1;
    p.attack = 0.0015; p.decay = 0.09; p.sustain = 0.0; p.release = 0.07;
    p.portamentoMode = 0;
}

inline void presetNoiseSnare(SH101Params& p) {
    p.noiseLevel = 1.0; p.sawLevel = 0.0; p.pulseLevel = 0.0; p.subLevel = 0.0;
    p.cutoff = 2800.0; p.resonance = 0.45; p.filterEnvAmt = 1.8;
    p.attack = 0.0015; p.decay = 0.13; p.sustain = 0.0; p.release = 0.08;
    p.vcaMode = 0;
    p.portamentoMode = 0;
}

// ---- Effects / textures -----------------------------------------------------
inline void presetSelfOscSiren(SH101Params& p) {
    p.sawLevel = 0.0; p.pulseLevel = 0.0; p.subLevel = 0.0; p.noiseLevel = 0.05;
    p.cutoff = 180.0; p.resonance = 1.0; p.keyTrack = 1.0;   // filter as oscillator
    p.filterEnvAmt = 0.0; p.filterModAmt = 1.2;
    p.lfoRate = 0.5; p.lfoWave = 0;
    p.attack = 0.08; p.decay = 3.0; p.sustain = 1.0; p.release = 0.5;
    p.portamentoMode = 0;
}

inline void presetNoiseSweep(SH101Params& p) {
    p.noiseLevel = 1.0; p.sawLevel = 0.0; p.pulseLevel = 0.0; p.subLevel = 0.0;
    p.cutoff = 60.0; p.resonance = 0.8; p.filterEnvAmt = 5.5; p.keyTrack = 0.0;
    p.attack = 1.6; p.decay = 3.5; p.sustain = 1.0; p.release = 1.2;
    p.portamentoMode = 0;
}

// ---- Arpeggiator / sequencer -----------------------------------------------
inline void presetArpClassic(SH101Params& p) {
    p.sawLevel = 1.0; p.pulseLevel = 0.0; p.subLevel = 0.4; p.subMode = 0;
    p.cutoff = 1500.0; p.resonance = 0.35; p.filterEnvAmt = 2.0; p.keyTrack = 0.4;
    p.attack = 0.003; p.decay = 0.22; p.sustain = 0.15; p.release = 0.1;
    p.arpOn = true; p.arpMode = 0; p.arpRate = 8.0; p.arpOctaves = 2;
    p.portamentoMode = 0;
}

inline void presetTechnoSequence(SH101Params& p) {
    p.pulseLevel = 1.0; p.sawLevel = 0.0; p.subLevel = 0.5; p.subMode = 0;
    p.pulseWidth = 0.35;
    p.cutoff = 680.0; p.resonance = 0.6; p.filterEnvAmt = 2.4; p.keyTrack = 0.45;
    p.attack = 0.002; p.decay = 0.16; p.sustain = 0.08; p.release = 0.08;
    p.seqOn = true; p.seqRate = 6.5;
    p.portamentoMode = 0;
}

inline void presetAcidSequence(SH101Params& p) {
    p.sawLevel = 1.0; p.pulseLevel = 0.0; p.subLevel = 0.15; p.subMode = 0;
    p.cutoff = 240.0; p.resonance = 0.85; p.filterEnvAmt = 3.6; p.keyTrack = 0.5;
    p.attack = 0.002; p.decay = 0.25; p.sustain = 0.05; p.release = 0.1;
    p.seqOn = true; p.seqRate = 7.5;
    p.portamentoMode = 0;
}

// ---- More bass ---------------------------------------------------------------
// The mixer's drive stage is what makes these different from the ones above:
// pushing the source levels into the filter input is the instrument's own
// distortion, not an added effect.
inline void presetDriveBass(SH101Params& p) {
    p.sawLevel = 1.0; p.pulseLevel = 0.8; p.subLevel = 0.5; p.subMode = 0;
    p.cutoff = 380.0; p.resonance = 0.45; p.filterEnvAmt = 2.8; p.keyTrack = 0.35;
    p.envTrigger = 0;
    p.attack = 0.002; p.decay = 0.22; p.sustain = 0.15; p.release = 0.1;
    p.volume = 0.65;   // the drive earns the level, so the output is dialled back
    p.portamentoMode = 0;
}

inline void presetDeepHouseBass(SH101Params& p) {
    p.pulseLevel = 1.0; p.sawLevel = 0.0; p.subLevel = 0.7; p.subMode = 0;
    p.pulseWidth = 0.5;
    p.cutoff = 260.0; p.resonance = 0.25; p.filterEnvAmt = 1.4; p.keyTrack = 0.3;
    p.attack = 0.006; p.decay = 0.5; p.sustain = 0.5; p.release = 0.3;
    p.portamentoMode = 0;
}

inline void presetWobbleBass(SH101Params& p) {
    p.sawLevel = 1.0; p.pulseLevel = 0.4; p.subLevel = 0.5; p.subMode = 0;
    p.cutoff = 300.0; p.resonance = 0.6; p.filterEnvAmt = 1.6;
    p.filterModAmt = 1.8; p.lfoRate = 3.2; p.lfoWave = 0;   // the wobble is the LFO
    p.attack = 0.004; p.decay = 0.4; p.sustain = 0.6; p.release = 0.2;
    p.portamentoMode = 0;
}

inline void presetSquelchLine(SH101Params& p) {
    p.pulseLevel = 1.0; p.sawLevel = 0.7; p.subLevel = 0.2; p.subMode = 0;
    p.pulseWidth = 0.35;
    p.cutoff = 150.0; p.resonance = 0.9; p.filterEnvAmt = 4.0; p.keyTrack = 0.55;
    p.envTrigger = 0;
    p.attack = 0.0015; p.decay = 0.3; p.sustain = 0.02; p.release = 0.1;
    p.portamentoMode = 0;
}

// ---- More lead ---------------------------------------------------------------
inline void presetVintageSolo(SH101Params& p) {
    p.pulseLevel = 1.0; p.sawLevel = 0.4; p.subLevel = 0.2; p.subMode = 0;
    p.pulseWidth = 0.45; p.pwmSource = 2; p.pwmAmount = 0.35; p.lfoRate = 5.2;
    p.cutoff = 1800.0; p.resonance = 0.35; p.filterEnvAmt = 1.2; p.keyTrack = 0.75;
    p.attack = 0.02; p.decay = 0.7; p.sustain = 0.85; p.release = 0.35;
    p.portamentoTime = 0.12; p.portamentoMode = 1;
}

inline void presetVibratoLead(SH101Params& p) {
    p.sawLevel = 1.0; p.subLevel = 0.2;
    p.vcoModDep = 0.08; p.lfoRate = 5.5; p.lfoWave = 0;    // gentle vibrato
    p.cutoff = 2600.0; p.resonance = 0.25; p.filterEnvAmt = 0.9; p.keyTrack = 0.8;
    p.attack = 0.03; p.decay = 0.6; p.sustain = 0.9; p.release = 0.3;
    p.portamentoMode = 0;
}

inline void presetGlassLead(SH101Params& p) {
    p.pulseLevel = 1.0; p.pulseWidth = 0.12; p.pwmSource = 1;
    p.vcoRange = 2;
    p.cutoff = 5200.0; p.resonance = 0.5; p.filterEnvAmt = 1.0; p.keyTrack = 0.9;
    p.attack = 0.004; p.decay = 0.5; p.sustain = 0.6; p.release = 0.4;
    p.volume = 0.7;
    p.portamentoMode = 0;
}

inline void presetFlute(SH101Params& p) {
    p.pulseLevel = 1.0; p.pulseWidth = 0.06; p.pwmSource = 1;   // narrow = hollow
    p.vcoModDep = 0.05; p.lfoRate = 5.0;
    p.cutoff = 3200.0; p.resonance = 0.1; p.filterEnvAmt = 0.4;
    p.attack = 0.08; p.decay = 0.5; p.sustain = 0.9; p.release = 0.25;
    p.noiseLevel = 0.04;   // a breath of noise
    p.portamentoMode = 0;
}

// ---- More pad / strings ------------------------------------------------------
inline void presetChoirPad(SH101Params& p) {
    p.pulseLevel = 1.0; p.sawLevel = 0.3; p.subLevel = 0.25;
    p.pulseWidth = 0.5; p.pwmSource = 2; p.pwmAmount = 0.9; p.lfoRate = 0.22;
    p.cutoff = 1500.0; p.resonance = 0.2; p.filterEnvAmt = 0.9;
    p.filterModAmt = 0.4; p.keyTrack = 0.45;
    p.attack = 0.7; p.decay = 1.6; p.sustain = 0.95; p.release = 1.4;
    p.volume = 0.72;
    p.portamentoMode = 0;
}

inline void presetDronePad(SH101Params& p) {
    p.sawLevel = 0.7; p.pulseLevel = 0.5; p.subLevel = 0.6; p.subMode = 1;
    p.cutoff = 700.0; p.resonance = 0.15; p.filterEnvAmt = 0.5;
    p.filterModAmt = 0.7; p.lfoRate = 0.09; p.lfoWave = 0;
    p.attack = 1.6; p.decay = 3.0; p.sustain = 1.0; p.release = 2.5;
    p.volume = 0.7;
    p.portamentoMode = 0;
}

inline void presetSlowSweepStrings(SH101Params& p) {
    p.pulseLevel = 1.0; p.sawLevel = 0.6; p.subLevel = 0.2;
    p.pulseWidth = 0.5; p.pwmSource = 2; p.pwmAmount = 0.6; p.lfoRate = 0.35;
    p.cutoff = 900.0; p.resonance = 0.3; p.filterEnvAmt = 2.2;   // slow filter sweep
    p.keyTrack = 0.4;
    p.attack = 0.9; p.decay = 2.0; p.sustain = 0.9; p.release = 1.6;
    p.portamentoMode = 0;
}

// ---- More pluck / percussive -------------------------------------------------
inline void presetWoodPluck(SH101Params& p) {
    p.pulseLevel = 1.0; p.pulseWidth = 0.3; p.pwmSource = 0; p.pwmAmount = 0.5;
    p.cutoff = 900.0; p.resonance = 0.55; p.filterEnvAmt = 2.4; p.keyTrack = 0.6;
    p.envTrigger = 1;
    p.attack = 0.0015; p.decay = 0.2; p.sustain = 0.0; p.release = 0.12;
    p.portamentoMode = 0;
}

inline void presetBellPluck(SH101Params& p) {
    p.pulseLevel = 1.0; p.pulseWidth = 0.08;
    p.vcoRange = 2;
    p.cutoff = 6000.0; p.resonance = 0.7; p.filterEnvAmt = 1.6; p.keyTrack = 1.0;
    p.attack = 0.0015; p.decay = 0.35; p.sustain = 0.0; p.release = 0.3;
    p.volume = 0.62;
    p.portamentoMode = 0;
}

inline void presetTom(SH101Params& p) {
    p.noiseLevel = 0.25; p.pulseLevel = 0.0; p.sawLevel = 0.0; p.subLevel = 1.0; p.subMode = 0;
    p.vcoRange = 0;
    p.cutoff = 400.0; p.resonance = 0.3; p.filterEnvAmt = 1.4;
    p.attack = 0.0015; p.decay = 0.28; p.sustain = 0.0; p.release = 0.15;
    p.portamentoMode = 0;
}

inline void presetHats(SH101Params& p) {
    p.noiseLevel = 1.0; p.sawLevel = 0.0; p.pulseLevel = 0.0; p.subLevel = 0.0;
    p.cutoff = 7000.0; p.resonance = 0.5; p.filterEnvAmt = 1.0; p.keyTrack = 0.8;
    p.attack = 0.0015; p.decay = 0.05; p.sustain = 0.0; p.release = 0.04;
    p.volume = 0.6;
    p.portamentoMode = 0;
}

// ---- More effects ------------------------------------------------------------
inline void presetWind(SH101Params& p) {
    p.noiseLevel = 1.0; p.sawLevel = 0.0; p.pulseLevel = 0.0; p.subLevel = 0.0;
    p.cutoff = 500.0; p.resonance = 0.95; p.filterEnvAmt = 0.0;
    p.filterModAmt = 2.2; p.lfoRate = 0.18; p.lfoWave = 0;   // resonant noise gusts
    p.attack = 0.8; p.decay = 3.0; p.sustain = 1.0; p.release = 1.5;
    p.volume = 0.7;
    p.portamentoMode = 0;
}

inline void presetLaserSweep(SH101Params& p) {
    p.sawLevel = 1.0; p.resonance = 0.95; p.cutoff = 120.0;
    p.filterEnvAmt = 5.5; p.keyTrack = 0.5;
    p.envTrigger = 0;
    p.attack = 0.0015; p.decay = 0.45; p.sustain = 0.0; p.release = 0.25;
    p.volume = 0.65;
    p.portamentoMode = 0;
}

inline void presetRadioStatic(SH101Params& p) {
    p.noiseLevel = 0.9; p.sawLevel = 0.15; p.pulseLevel = 0.0; p.subLevel = 0.0;
    p.cutoff = 1800.0; p.resonance = 0.85;
    p.filterModAmt = 3.0; p.lfoRate = 12.0; p.lfoWave = 2;   // stepped random
    p.attack = 0.0015; p.decay = 0.4; p.sustain = 0.7; p.release = 0.2;
    p.volume = 0.6;
    p.portamentoMode = 0;
}

// ---- More arpeggiator / sequencer -------------------------------------------
inline void presetArpDown(SH101Params& p) {
    p.pulseLevel = 1.0; p.sawLevel = 0.3; p.subLevel = 0.35; p.subMode = 0;
    p.pulseWidth = 0.4;
    p.cutoff = 1700.0; p.resonance = 0.4; p.filterEnvAmt = 1.8; p.keyTrack = 0.45;
    p.attack = 0.003; p.decay = 0.24; p.sustain = 0.1; p.release = 0.12;
    p.arpOn = true; p.arpMode = 1; p.arpRate = 9.0; p.arpOctaves = 2;
    p.portamentoMode = 0;
}

inline void presetArpUpDown(SH101Params& p) {
    p.pulseLevel = 1.0; p.pulseWidth = 0.5; p.pwmSource = 2; p.pwmAmount = 0.5;
    p.lfoRate = 1.2;
    p.cutoff = 2400.0; p.resonance = 0.3; p.filterEnvAmt = 1.4; p.keyTrack = 0.5;
    p.attack = 0.004; p.decay = 0.3; p.sustain = 0.25; p.release = 0.18;
    p.arpOn = true; p.arpMode = 2; p.arpRate = 6.5; p.arpOctaves = 3;
    p.portamentoMode = 0;
}

inline void presetArpTrance(SH101Params& p) {
    p.sawLevel = 1.0; p.subLevel = 0.3;
    p.cutoff = 3000.0; p.resonance = 0.45; p.filterEnvAmt = 1.2;
    p.filterModAmt = 0.8; p.lfoRate = 0.5;
    p.attack = 0.002; p.decay = 0.16; p.sustain = 0.05; p.release = 0.1;
    p.arpOn = true; p.arpMode = 0; p.arpRate = 12.0; p.arpOctaves = 2;
    p.portamentoMode = 0;
}

inline void presetElectroSequence(SH101Params& p) {
    p.sawLevel = 0.9; p.pulseLevel = 0.6; p.subLevel = 0.6; p.subMode = 0;
    p.cutoff = 900.0; p.resonance = 0.5; p.filterEnvAmt = 2.6; p.keyTrack = 0.4;
    p.attack = 0.0015; p.decay = 0.14; p.sustain = 0.0; p.release = 0.08;
    p.seqOn = true; p.seqRate = 8.0;
    p.portamentoMode = 0;
}

// ---- Sequence patterns for the sequenced presets ---------------------------
// Eight steps each, written the way the hardware sequence memories hold them:
// note, gate (false = rest) and tie (hold into the next step).

// Driving on-beat pulse line with an octave jump and a rest at the end.
inline void patternTechnoSequence(StepSequencer& seq) {
    static const int steps[8][3] = {
        { 36, 1, 0 }, { 36, 1, 1 }, { 48, 1, 0 }, { 36, 1, 0 },
        { 43, 1, 0 }, { 36, 1, 1 }, { 48, 1, 0 }, { 0, 0, 0 },
    };
    seq.setLength(8);
    for (int i = 0; i < 8; ++i) {
        seq.setStep(i, steps[i][0] == 0 ? 36 : steps[i][0], steps[i][1] != 0, steps[i][2] != 0);
    }
}

// Resonant saw line with a turn-around, the classic octave/fourth acid figure.
inline void patternAcidSequence(StepSequencer& seq) {
    static const int steps[8][3] = {
        { 36, 1, 0 }, { 36, 1, 0 }, { 48, 1, 0 }, { 39, 1, 0 },
        { 36, 1, 0 }, { 51, 1, 0 }, { 48, 1, 1 }, { 43, 0, 0 },
    };
    seq.setLength(8);
    for (int i = 0; i < 8; ++i) {
        seq.setStep(i, steps[i][0] == 0 ? 36 : steps[i][0], steps[i][1] != 0, steps[i][2] != 0);
    }
}

// Off-beat electro line: syncopated, with the accent on the "and".
inline void patternElectroSequence(StepSequencer& seq) {
    static const int steps[8][3] = {
        { 36, 1, 0 }, { 0, 0, 0 }, { 48, 1, 0 }, { 36, 1, 0 },
        { 43, 1, 1 }, { 0, 0, 0 }, { 55, 1, 0 }, { 46, 1, 0 },
    };
    seq.setLength(8);
    for (int i = 0; i < 8; ++i) {
        seq.setStep(i, steps[i][0] == 0 ? 36 : steps[i][0], steps[i][1] != 0, steps[i][2] != 0);
    }
}

// ---- The bank ---------------------------------------------------------------
// Order matters: this is the order the editor's preset selector presents.
inline const PresetInfo* presetBank(int& count) {
    static const PresetInfo kPresets[] = {
        { "Init",  "Init",              "Reference patch: pulse oscillator, filter open, no envelope amount", presetInit },
        // ---- Bass ----------------------------------------------------------
        { "Bass",  "Sub Bass",          "Sub oscillator low end, gentle filter movement",                     presetSubBass },
        { "Bass",  "Acid Bass",         "Resonant saw with a fast, deep filter sweep on every note",          presetAcidBass },
        { "Bass",  "Squelch Line",      "Maximum resonance on a short, punchy line",                          presetSquelchLine },
        { "Bass",  "Rubber Bass",       "Narrow pulse, resonant and bouncy",                                  presetRubberBass },
        { "Bass",  "Electro Bass",      "Short, punchy saw+sub stab",                                         presetElectroBass },
        { "Bass",  "Chicago Bass",      "Warm pulse bass with a slow filter wobble",                          presetChicagoBass },
        { "Bass",  "Deep House Bass",   "Round pulse sub with a long, soft tail",                             presetDeepHouseBass },
        { "Bass",  "Drive Bass",        "Everything into the mixer: the filter input itself distorts",       presetDriveBass },
        { "Bass",  "Wobble Bass",       "Filter LFO sweeping a saw+sub bass",                                 presetWobbleBass },
        // ---- Lead ----------------------------------------------------------
        { "Lead",  "Classic Lead",      "Bright saw lead with a touch of glide",                              presetClassicLead },
        { "Lead",  "Squelch Lead",      "Resonant, vocal filter sweep",                                       presetSquelchLead },
        { "Lead",  "Vintage Solo",      "Gliding pulse solo voice with PWM movement",                         presetVintageSolo },
        { "Lead",  "Vibrato Lead",      "Saw lead with a gentle LFO vibrato",                                 presetVibratoLead },
        { "Lead",  "PWM Lead",          "Pulse lead animated by LFO pulse-width modulation",                  presetPwmLead },
        { "Lead",  "Whistle Lead",      "Thin 4' pulse whistle with full key tracking",                       presetWhistleLead },
        { "Lead",  "Glass Lead",        "Very narrow high pulse, glassy and clean",                           presetGlassLead },
        { "Lead",  "Flute",             "Hollow narrow pulse with vibrato and a breath of noise",             presetFlute },
        // ---- Pad / strings -------------------------------------------------
        { "Pad",   "PWM Strings",       "Classic slow pulse-width strings",                                   presetPwmStrings },
        { "Pad",   "Vintage Strings",   "Pulse + saw string ensemble character",                              presetVintageStrings },
        { "Pad",   "Choir Pad",         "Wide, slow PWM with a long release",                                 presetChoirPad },
        { "Pad",   "Slow Sweep Strings","Strings behind a slow filter sweep",                                 presetSlowSweepStrings },
        { "Pad",   "Drone Pad",         "Very slow attack, moving drone",                                     presetDronePad },
        { "Pad",   "Soft Pad",          "Slow attack, gently breathing filter",                               presetSoftPad },
        // ---- Pluck / percussive --------------------------------------------
        { "Pluck", "Synth Pluck",       "One filter sweep per key press, no sustain",                         presetSynthPluck },
        { "Pluck", "Sequence Blip",     "Very short narrow-pulse blip for patterns",                          presetSequenceBlip },
        { "Pluck", "Wood Pluck",        "Envelope-modulated pulse, woody and dry",                            presetWoodPluck },
        { "Pluck", "Bell Pluck",        "High, resonant narrow pulse with a fast decay",                      presetBellPluck },
        { "Perc",  "Noise Snare",       "Filtered noise burst, envelope on the VCA",                          presetNoiseSnare },
        { "Perc",  "Tom",               "Tuned sub thump with a touch of noise",                              presetTom },
        { "Perc",  "Hats",              "Very short high-passed noise",                                       presetHats },
        // ---- Effects -------------------------------------------------------
        { "FX",    "Self-Osc Siren",    "Filter at full resonance used as an oscillator",                     presetSelfOscSiren },
        { "FX",    "Wind",              "Resonant noise gusts under a slow LFO",                              presetWind },
        { "FX",    "Noise Sweep",       "Slow opening noise sweep",                                           presetNoiseSweep },
        { "FX",    "Laser Sweep",       "Deep resonant sweep on every note",                                  presetLaserSweep },
        { "FX",    "Radio Static",      "Stepped random modulation on resonant noise",                        presetRadioStatic },
        // ---- Arpeggiator ---------------------------------------------------
        { "Arp",   "Arp Classic",       "Up arpeggio, two octaves, turned on",                                 presetArpClassic },
        { "Arp",   "Arp Down",          "Descending arpeggio",                                                presetArpDown },
        { "Arp",   "Arp Up/Down",       "Up-down arpeggio over three octaves",                                presetArpUpDown },
        { "Arp",   "Arp Trance",        "Fast two-octave up arpeggio with a moving filter",                   presetArpTrance },
        // ---- Sequencer -----------------------------------------------------
        { "Seq",   "Techno Sequence",   "Sequencer running on a driving pulse",                                presetTechnoSequence, patternTechnoSequence },
        { "Seq",   "Acid Sequence",     "Sequencer running on a resonant saw",                                 presetAcidSequence, patternAcidSequence },
        { "Seq",   "Electro Sequence",  "Syncopated electro line with rests and ties",                        presetElectroSequence, patternElectroSequence },
    };
    count = static_cast<int>(sizeof(kPresets) / sizeof(kPresets[0]));
    return kPresets;
}

inline int numPresets() {
    int count = 0;
    presetBank(count);
    return count;
}

inline const PresetInfo& preset(int index) {
    int count = 0;
    const PresetInfo* bank = presetBank(count);
    if (index < 0) index = 0;
    if (index >= count) index = count - 1;
    return bank[index];
}

// The parameter set a preset stands for: the reference patch, then the preset's
// own changes.
inline SH101Params makePresetParams(int index) {
    SH101Params p;
    preset(index).apply(p);
    return p;
}

// Case-insensitive lookup by name, for command-line and host convenience.
// Returns -1 when the name is not in the bank.
inline int findPresetByName(const char* name) {
    if (name == nullptr) return -1;
    int count = 0;
    const PresetInfo* bank = presetBank(count);
    for (int i = 0; i < count; ++i) {
        if (std::strcmp(bank[i].name, name) == 0) return i;
    }
    for (int i = 0; i < count; ++i) {
        // Fall back to a case-insensitive, space-tolerant comparison so
        // "acid bass" and "acidbass" both find "Acid Bass".
        const char* a = bank[i].name;
        const char* b = name;
        bool match = true;
        while (*a != '\0' || *b != '\0') {
            while (*a == ' ') ++a;
            while (*b == ' ') ++b;
            const char ca = (*a >= 'A' && *a <= 'Z') ? static_cast<char>(*a - 'A' + 'a') : *a;
            const char cb = (*b >= 'A' && *b <= 'Z') ? static_cast<char>(*b - 'A' + 'a') : *b;
            if (ca != cb) { match = false; break; }
            if (*a != '\0') ++a;
            if (*b != '\0') ++b;
        }
        if (match) return i;
    }
    return -1;
}

} // namespace sh101
