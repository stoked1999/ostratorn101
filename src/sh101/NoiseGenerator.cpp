#include "sh101/NoiseGenerator.h"

namespace sh101 {

void NoiseGenerator::prepare(double sampleRate, const Calibration& cal) {
    sr_ = sampleRate > 0.0 ? sampleRate : 48000.0;
    cal_ = cal;
    updateFilters();
    reset();
}

void NoiseGenerator::updateFilters() {
    // One-pole high-pass corner taken from the conditioning-network trim.
    const double fc = clampd(cal_.noiseShelfHz, 0.1, sr_ * 0.4);
    hpCoef_ = std::exp(-kTwoPi * fc / sr_);
}

void NoiseGenerator::reset(uint32_t seed) {
    rng_.reset(seed);
    hpState_ = 0.0;
    hpInputPrev_ = 0.0;
    white_ = 0.0;
    conditioned_ = 0.0;
}

double NoiseGenerator::process() {
    white_ = rng_.nextBipolar() * cal_.noiseAmp;

    // One-pole high-pass (the conditioning stage's low-frequency roll-off):
    //   y[n] = a * (y[n-1] + x[n] - x[n-1])
    const double y = hpCoef_ * (hpState_ + white_ - hpInputPrev_);
    hpInputPrev_ = white_;
    hpState_ = flushDenormal(y);
    conditioned_ = hpState_;

    return conditioned_;
}

} // namespace sh101
