// Parameter definitions, ranges and tapers.
//
// The brief asks for normalized 0..1 plugin parameters mapped to the original
// ranges *with the original tapers* — not linearly.  Slider tapers live here so
// the DSP never has to know about them.
#pragma once

#include "sh101/Constants.h"

namespace sh101 {

enum ParamId {
    pLfoRate = 0,
    pLfoWave,
    pVcoRange,
    pVcoTune,
    pVcoModDepth,
    pPulseWidth,
    pPwmSource,
    pPwmAmount,
    pSawLevel,
    pPulseLevel,
    pSubLevel,
    pSubMode,
    pNoiseLevel,
    pCutoff,
    pResonance,
    pFilterEnvAmount,
    pFilterModAmount,
    pKeyTrack,
    pAttack,
    pDecay,
    pSustain,
    pRelease,
    pEnvTrigger,
    pVcaMode,
    pPortamentoTime,
    pPortamentoMode,
    pVolume,
    pBend,          // bender / mod grip pitch amount, -2..+2 semitones
    pArpOn,
    pArpMode,
    pArpRate,
    pArpOctaves,
    pSeqOn,
    pSeqRate,
    kNumParams
};

// ---- Taper helpers ---------------------------------------------------------

// Exponential ("audio") taper between two positive limits.
inline double taperExponential(double n, double lo, double hi) {
    n = clampd(n, 0.0, 1.0);
    return lo * std::pow(hi / lo, n);
}

// Linear taper.
inline double taperLinear(double n, double lo, double hi) {
    return lo + (hi - lo) * clampd(n, 0.0, 1.0);
}

// Inverse of taperExponential, for UI read-back / tests.
inline double taperExponentialInverse(double v, double lo, double hi) {
    v = clampd(v, lo, hi);
    return std::log(v / lo) / std::log(hi / lo);
}

inline void snapToInt(double& v, int lo, int hi) {
    v = static_cast<double>(clampi(static_cast<int>(v + 0.5), lo, hi));
}

// ---- The parameter set -----------------------------------------------------
//
// Values are stored in engineering units (Hz, seconds, volts) after the taper
// has been applied, so DSP code reads e.g. `p.cutoff` as Hz.
struct SH101Params {
    // LFO / modulator
    double lfoRate    = 5.0;      // 0.1 .. 30 Hz
    int    lfoWave    = 0;        // 0 triangle, 1 square, 2 random, 3 noise
    // VCO
    int    vcoRange   = 1;        // 0=16', 1=8', 2=4', 3=2'
    double vcoTune    = 0.0;      // cents, +/-50
    double vcoModDep  = 0.0;      // 0..1 LFO -> pitch modulation depth
    double pulseWidth = 0.5;      // manual pulse width, 0.03..0.97
    int    pwmSource  = 1;        // 0=ENV, 1=MANUAL, 2=LFO
    double pwmAmount  = 0.0;      // modulation depth into the pulse width
    // Source mixer
    double sawLevel   = 0.0;
    double pulseLevel = 1.0;
    double subLevel   = 0.0;
    double noiseLevel = 0.0;
    int    subMode    = 0;        // 0=-1 oct sq, 1=-2 oct sq, 2=-2 oct narrow
    // VCF
    double cutoff        = 1200.0; // Hz, 10 Hz .. 20 kHz
    double resonance     = 0.0;    // 0..1, 1.0 = self-oscillation
    double filterEnvAmt  = 0.0;    // octaves
    double filterModAmt  = 0.0;    // octaves (LFO)
    double keyTrack      = 0.0;    // 0..1
    // Envelope
    double attack  = 0.005;  // s, 0.0015 .. 4
    double decay   = 0.30;   // s, 0.002 .. 10
    double sustain = 0.70;   // 0..1
    double release = 0.30;   // s, 0.002 .. 10
    int    envTrigger = 0;   // 0=GATE+TRIG, 1=GATE, 2=LFO
    // VCA
    int    vcaMode = 0;      // 0=ENV, 1=GATE
    // Portamento
    double portamentoTime = 0.0; // 0..5 s
    int    portamentoMode = 1;   // 0=OFF, 1=ON, 2=AUTO
    // Output
    double volume = 0.8;
    double bendSemitones = 0.0;  // -2..+2
    // Arpeggiator / sequencer
    bool   arpOn = false;
    int    arpMode = 0;      // 0=up, 1=down, 2=up/down
    double arpRate = 5.0;    // Hz clock
    int    arpOctaves = 1;   // 1..3
    bool   seqOn = false;
    double seqRate = 5.0;    // Hz clock
};

// ---- Normalized (0..1) <-> engineering mapping ------------------------------
inline void applyNormalized(SH101Params& p, int id, double n) {
    n = clampd(n, 0.0, 1.0);
    switch (id) {
        case pLfoRate:        p.lfoRate = taperExponential(n, 0.1, 30.0); break;
        case pLfoWave:        p.lfoWave = clampi((int)(n * 3.999), 0, 3); break;
        case pVcoRange:       p.vcoRange = clampi((int)(n * 3.999), 0, 3); break;
        case pVcoTune:        p.vcoTune = taperLinear(n, -50.0, 50.0); break;
        case pVcoModDepth:    p.vcoModDep = n; break;
        // Pulse width slider: fully clockwise = square (50%), fully counter-
        // clockwise = narrow.  The hardware never reaches 0% duty.
        case pPulseWidth:     p.pulseWidth = taperLinear(n, 0.03, 0.50); break;
        case pPwmSource:      p.pwmSource = clampi((int)(n * 2.999), 0, 2); break;
        case pPwmAmount:      p.pwmAmount = n; break;
        case pSawLevel:       p.sawLevel = n; break;
        case pPulseLevel:     p.pulseLevel = n; break;
        case pSubLevel:       p.subLevel = n; break;
        case pSubMode:        p.subMode = clampi((int)(n * 2.999), 0, 2); break;
        case pNoiseLevel:     p.noiseLevel = n; break;
        case pCutoff:         p.cutoff = taperExponential(n, 10.0, 20000.0); break;
        case pResonance:      p.resonance = n; break;
        case pFilterEnvAmount:p.filterEnvAmt = taperLinear(n, 0.0, 6.0); break;
        case pFilterModAmount:p.filterModAmt = taperLinear(n, 0.0, 6.0); break;
        case pKeyTrack:       p.keyTrack = n; break;
        // Envelope times are exponential, matching how the analog RC networks
        // behave; documented ranges from the operator manual.
        case pAttack:         p.attack = taperExponential(n, 0.0015, 4.0); break;
        case pDecay:          p.decay = taperExponential(n, 0.002, 10.0); break;
        case pSustain:        p.sustain = n; break;
        case pRelease:        p.release = taperExponential(n, 0.002, 10.0); break;
        case pEnvTrigger:     p.envTrigger = clampi((int)(n * 2.999), 0, 2); break;
        case pVcaMode:        p.vcaMode = clampi((int)(n * 1.999), 0, 1); break;
        case pPortamentoTime: p.portamentoTime = taperLinear(n, 0.0, 5.0); break;
        case pPortamentoMode: p.portamentoMode = clampi((int)(n * 2.999), 0, 2); break;
        case pVolume:         p.volume = n; break;
        case pBend:           p.bendSemitones = taperLinear(n, -2.0, 2.0); break;
        case pArpOn:          p.arpOn = (n >= 0.5); break;
        case pArpMode:        p.arpMode = clampi((int)(n * 2.999), 0, 2); break;
        case pArpRate:        p.arpRate = taperExponential(n, 0.5, 30.0); break;
        case pArpOctaves:     p.arpOctaves = clampi(1 + (int)(n * 2.999), 1, 3); break;
        case pSeqOn:          p.seqOn = (n >= 0.5); break;
        case pSeqRate:        p.seqRate = taperExponential(n, 0.5, 30.0); break;
        default: break;
    }
}

inline double getNormalized(const SH101Params& p, int id) {
    switch (id) {
        case pLfoRate:        return taperExponentialInverse(p.lfoRate, 0.1, 30.0);
        case pLfoWave:        return p.lfoWave / 3.0;
        case pVcoRange:       return p.vcoRange / 3.0;
        case pVcoTune:        return (p.vcoTune + 50.0) / 100.0;
        case pVcoModDepth:    return p.vcoModDep;
        case pPulseWidth:     return (p.pulseWidth - 0.03) / (0.50 - 0.03);
        case pPwmSource:      return p.pwmSource / 2.0;
        case pPwmAmount:      return p.pwmAmount;
        case pSawLevel:       return p.sawLevel;
        case pPulseLevel:     return p.pulseLevel;
        case pSubLevel:       return p.subLevel;
        case pSubMode:        return p.subMode / 2.0;
        case pNoiseLevel:     return p.noiseLevel;
        case pCutoff:         return taperExponentialInverse(p.cutoff, 10.0, 20000.0);
        case pResonance:      return p.resonance;
        case pFilterEnvAmount:return p.filterEnvAmt / 6.0;
        case pFilterModAmount:return p.filterModAmt / 6.0;
        case pKeyTrack:       return p.keyTrack;
        case pAttack:         return taperExponentialInverse(p.attack, 0.0015, 4.0);
        case pDecay:          return taperExponentialInverse(p.decay, 0.002, 10.0);
        case pSustain:        return p.sustain;
        case pRelease:        return taperExponentialInverse(p.release, 0.002, 10.0);
        case pEnvTrigger:     return p.envTrigger / 2.0;
        case pVcaMode:        return p.vcaMode;
        case pPortamentoTime: return p.portamentoTime / 5.0;
        case pPortamentoMode: return p.portamentoMode / 2.0;
        case pVolume:         return p.volume;
        case pBend:           return (p.bendSemitones + 2.0) / 4.0;
        case pArpOn:          return p.arpOn ? 1.0 : 0.0;
        case pArpMode:        return p.arpMode / 2.0;
        case pArpRate:        return taperExponentialInverse(p.arpRate, 0.5, 30.0);
        case pArpOctaves:     return (p.arpOctaves - 1) / 2.0;
        case pSeqOn:          return p.seqOn ? 1.0 : 0.0;
        case pSeqRate:        return taperExponentialInverse(p.seqRate, 0.5, 30.0);
        default: return 0.0;
    }
}

inline const char* paramName(int id) {
    static const char* names[kNumParams] = {
        "lfoRate", "lfoWave", "vcoRange", "vcoTune", "vcoModDepth", "pulseWidth",
        "pwmSource", "pwmAmount", "sawLevel", "pulseLevel", "subLevel", "subMode",
        "noiseLevel", "cutoff", "resonance", "filterEnvAmount", "filterModAmount",
        "keyTrack", "attack", "decay", "sustain", "release", "envTrigger", "vcaMode",
        "portamentoTime", "portamentoMode", "volume", "bend", "arpOn", "arpMode",
        "arpRate", "arpOctaves", "seqOn", "seqRate"
    };
    return (id >= 0 && id < kNumParams) ? names[id] : "?";
}

// ---- Named positions for the switch-like controls ---------------------------
// These controls are steps in the hardware, not continuous knobs.  The tables
// below give each position a name so hosts and the editor can present them as
// choices rather than as anonymous 0..1 faders.  The DSP keeps using the
// normalized 0..1 value (applyNormalized maps it to the position), so nothing in
// the audio path depends on the UI representation.
struct ParamChoice {
    int paramId;
    const char* const* items;
    int count;
};

inline const char* const* kLfoWaveItems(int& n) {
    static const char* items[] = { "Triangle", "Square", "Random", "Noise" };
    n = 4;
    return items;
}
inline const char* const* kVcoRangeItems(int& n) {
    static const char* items[] = { "16'", "8'", "4'", "2'" };
    n = 4;
    return items;
}
inline const char* const* kSubModeItems(int& n) {
    static const char* items[] = { "-1 Oct Square", "-2 Oct Square", "-2 Oct Narrow" };
    n = 3;
    return items;
}
inline const char* const* kPwmSourceItems(int& n) {
    static const char* items[] = { "ENV", "Manual", "LFO" };
    n = 3;
    return items;
}
inline const char* const* kEnvTriggerItems(int& n) {
    static const char* items[] = { "Gate + Trig", "Gate", "LFO" };
    n = 3;
    return items;
}
inline const char* const* kVcaModeItems(int& n) {
    static const char* items[] = { "ENV", "Gate" };
    n = 2;
    return items;
}
inline const char* const* kPortamentoModeItems(int& n) {
    static const char* items[] = { "Off", "On", "Auto" };
    n = 3;
    return items;
}
inline const char* const* kArpModeItems(int& n) {
    static const char* items[] = { "Up", "Down", "Up/Down" };
    n = 3;
    return items;
}
inline const char* const* kOnOffItems(int& n) {
    static const char* items[] = { "Off", "On" };
    n = 2;
    return items;
}

// Returns the choice list for a switch-like parameter, or nullptr for
// continuous parameters.
inline const char* const* paramChoiceItems(int paramId, int& count) {
    count = 0;
    switch (paramId) {
        case pLfoWave:        return kLfoWaveItems(count);
        case pVcoRange:       return kVcoRangeItems(count);
        case pSubMode:        return kSubModeItems(count);
        case pPwmSource:      return kPwmSourceItems(count);
        case pEnvTrigger:     return kEnvTriggerItems(count);
        case pVcaMode:        return kVcaModeItems(count);
        case pPortamentoMode: return kPortamentoModeItems(count);
        case pArpMode:        return kArpModeItems(count);
        case pArpOn:
        case pSeqOn:          return kOnOffItems(count);
        default:              return nullptr;
    }
}

inline bool isChoiceParameter(int paramId) {
    int count = 0;
    return paramChoiceItems(paramId, count) != nullptr;
}

} // namespace sh101
