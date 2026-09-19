// SH-101 VST emulation — shared constants and unit conversion helpers.
//
// References in this project are to the Roland SH-101 owner's/service manual
// scan (combined PDF, "SH-101 Owners & Service Manuals"), service-note pages
// ~28-35, and to the block list in the engineering brief.
#pragma once

#include <algorithm>
#include <cmath>
#include <cstdint>

namespace sh101 {

constexpr double kPi    = 3.14159265358979323846;
constexpr double kTwoPi = 6.28318530717958647692;

// The instrument's control standard is 1 V/octave (service notes: CV standard
// 1 V/oct).  Internally every pitch- and cutoff-related quantity is carried as
// a floating-point "volt" value; conversion to Hz happens as late as possible
// (brief: "Keep modulation in pitch/CV units until the last conversion").
//
// 0 V == MIDI note 60 (C4) == 261.6256 Hz, the same reference the SH-101's
// D/A-derived keyboard CV uses after calibration ("D/A tune"/"D/A width").
constexpr double kRefFreqC4 = 261.6255653005986;

inline double midiNoteToCV(double midiNote) { return (midiNote - 60.0) / 12.0; }
inline double cvToFrequency(double cv)      { return kRefFreqC4 * std::exp2(cv); }
inline double frequencyToCV(double hz)      { return std::log2(hz / kRefFreqC4); }

inline double clampd(double v, double lo, double hi) {
    return v < lo ? lo : (v > hi ? hi : v);
}
inline int clampi(int v, int lo, int hi) { return v < lo ? lo : (v > hi ? hi : v); }

inline bool isFiniteNum(double v) { return std::isfinite(v); }

// Denormals are expensive and can appear in exponentially decaying filter
// states; flush them (brief: "no NaN/Inf/denormal failures").
inline double flushDenormal(double v) {
    return (v < 1.0e-30 && v > -1.0e-30) ? 0.0 : v;
}

// Windowed-sinc free polyBLEP residual (band-limiting helper, see SH101VCO).
// t is the oscillator phase in [0,1), dt = f/fs.
inline double polyBlep(double t, double dt) {
    if (dt <= 0.0) return 0.0;
    if (t < dt) {
        const double x = t / dt;
        return x + x - x * x - 1.0;
    }
    if (t > 1.0 - dt) {
        const double x = (t - 1.0) / dt;
        return x * x + x + x + 1.0;
    }
    return 0.0;
}

// Small deterministic PRNG (xorshift32).  Used for the noise generator and the
// random LFO; seeding is explicit so test renders are reproducible.
class XorShift32 {
public:
    explicit XorShift32(uint32_t seed = 0x1234567u) { reset(seed); }
    void reset(uint32_t seed) { s_ = seed ? seed : 0x9e3779b9u; }
    uint32_t next() {
        s_ ^= s_ << 13;
        s_ ^= s_ >> 17;
        s_ ^= s_ << 5;
        return s_;
    }
    // Uniform in [-1, 1)
    double nextBipolar() {
        return (static_cast<double>(next()) * (2.0 / 4294967296.0)) - 1.0;
    }
    // Uniform in [0, 1)
    double nextUnipolar() { return static_cast<double>(next()) * (1.0 / 4294967296.0); }
private:
    uint32_t s_ = 0x1234567u;
};

} // namespace sh101
