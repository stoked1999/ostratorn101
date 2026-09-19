// AnalogVariation — the small imperfections of a healthy analogue instrument.
//
// The brief asks for realistic analog variation and explicitly warns against
// "exaggerated random drift or instability"; the target is a well-calibrated
// unit, not a worn one.  What is modelled here is exactly that tolerance budget:
//
//   * keyboard CV / D-A tolerance — a small, fixed pitch error per note,
//   * VCO thermal drift — a slow, bounded random walk around the tuned pitch,
//   * VCF control-path drift — the same, on the cutoff,
//   * RC tolerance — a small per-note variation of the envelope segment times,
//   * the analogue noise floor — the hiss the output stage always has.
//
// Determinism: everything is derived from one xorshift32 stream, so a given seed
// reproduces a performance exactly.  The engine's deterministic test mode relies
// on that, and so does block-size independence — which is why the drift is
// advanced once per *sample* rather than once per block.
//
// Cost: two one-pole filters and one xorshift step per sample, no allocation.
#pragma once

#include "sh101/Calibration.h"
#include "sh101/Constants.h"

namespace sh101 {

class AnalogVariation {
public:
    void prepare(double sampleRate, const Calibration& cal) {
        cal_ = cal;
        sr_ = (sampleRate > 0.0) ? sampleRate : 48000.0;
        updateCoefficients();
    }

    void setCalibration(const Calibration& cal) {
        cal_ = cal;
        updateCoefficients();
    }

    void reset(uint32_t seed) {
        rng_ = (seed != 0u) ? seed : 0x9e3779b9u;
        driftPitch_ = 0.0;
        driftCutoff_ = 0.0;
        notePitchCents_ = 0.0;
        timeScale_ = 1.0;
        hiss_ = 0.0;
    }

    // A new note draws fresh per-note values: this key press's CV tolerance and
    // this key press's RC tolerance.
    void noteOn() {
        notePitchCents_ = cal_.noteToleranceCents * uniformSigned();
        timeScale_ = 1.0 + cal_.envTimeTolerance * uniformSigned();
    }

    // Advances the slow drifts and the noise floor by one sample.
    void process() {
        driftPitch_ += driftCoeffPitch_ * (uniformSigned() * driftGainPitch_ - driftPitch_);
        driftCutoff_ += driftCoeffCutoff_ * (uniformSigned() * driftGainCutoff_ - driftCutoff_);
        hiss_ = uniformSigned() * hissScale_;
    }

    bool enabled() const { return cal_.analogVariationEnabled; }

    // Slow drift + this note's fixed tolerance, in cents.
    double pitchOffsetCents() const {
        return enabled() ? clampd(driftPitch_, -1.0, 1.0) * cal_.driftPitchCents + notePitchCents_ : 0.0;
    }

    // Cutoff drift in octaves (the filter CV path shares the VCO's rails).
    double cutoffOffsetOctaves() const {
        return enabled() ? clampd(driftCutoff_, -1.0, 1.0) * cal_.driftCutoffOctaves : 0.0;
    }

    // Multiplies the envelope's segment times for the note in progress.
    double envelopeTimeScale() const { return enabled() ? timeScale_ : 1.0; }

    // The output stage's idle noise (added after the VCA, as on the board).
    double noiseFloor() const { return enabled() ? hiss_ : 0.0; }

    // RMS of the hiss this calibration produces, for tests and documentation.
    double noiseFloorRms() const { return enabled() ? hissScale_ / std::sqrt(3.0) : 0.0; }

    // Diagnostics for tests: the drift actually applied (clamped to the budget).
    double pitchDriftCents() const { return clampd(driftPitch_, -1.0, 1.0) * cal_.driftPitchCents; }
    double noteToleranceAppliedCents() const { return notePitchCents_; }
    double cutoffDriftOctaves() const { return clampd(driftCutoff_, -1.0, 1.0) * cal_.driftCutoffOctaves; }

private:
    void updateCoefficients() {
        // First-order low-pass corners taken from the calibrated drift rates.
        driftCoeffPitch_ = onePoleCoefficient(cal_.driftPitchHz);
        driftCoeffCutoff_ = onePoleCoefficient(cal_.driftCutoffHz);
        // A one-pole fed with white noise comes out far quieter than its input,
        // by sqrt(a/(2-a)) * (1/sqrt(3)) for uniform noise.  Each walk is scaled
        // by the reciprocal of that, so the *calibrated* figure is the walk's
        // unit standard deviation and its clamped peak is the figure itself.
        driftGainPitch_ = walkGain(driftCoeffPitch_);
        driftGainCutoff_ = walkGain(driftCoeffCutoff_);
        // noiseFloorDbfs is the *RMS* of the hiss, which is what a measurement
        // (and the test) sees; the uniform generator's peak is sqrt(3) higher.
        hissScale_ = std::sqrt(3.0) * std::pow(10.0, cal_.noiseFloorDbfs / 20.0);
    }

    static double walkGain(double coeff) {
        const double a = clampd(coeff, 1.0e-9, 1.0);
        const double attenuation = std::sqrt(a / (2.0 - a)) * (1.0 / std::sqrt(3.0));
        return (attenuation > 1.0e-12) ? (1.0 / attenuation) : 1.0;
    }

    double onePoleCoefficient(double frequencyHz) const {
        if (frequencyHz <= 0.0) return 1.0;
        const double x = kTwoPi * frequencyHz / sr_;
        return clampd(1.0 - std::exp(-x), 1.0e-9, 1.0);
    }

    // Uniform in [-1, 1] from a xorshift32 stream.  Deterministic, no allocation.
    double uniformSigned() {
        rng_ ^= rng_ << 13;
        rng_ ^= rng_ >> 17;
        rng_ ^= rng_ << 5;
        const double unit = static_cast<double>(rng_ >> 8) * (1.0 / 16777216.0);   // [0,1)
        return unit * 2.0 - 1.0;
    }

    Calibration cal_{};
    double sr_ = 48000.0;
    uint32_t rng_ = 0x9e3779b9u;

    double driftCoeffPitch_ = 0.0;
    double driftCoeffCutoff_ = 0.0;
    double driftGainPitch_ = 1.0;
    double driftGainCutoff_ = 1.0;
    double hissScale_ = 0.0;

    double driftPitch_ = 0.0;      // normalised walk, clamped to +/-1 on read
    double driftCutoff_ = 0.0;
    double notePitchCents_ = 0.0;  // this note's fixed CV tolerance
    double timeScale_ = 1.0;       // this note's envelope time multiplier
    double hiss_ = 0.0;
};

} // namespace sh101
