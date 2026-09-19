#include "sh101/SH101LFO.h"

namespace sh101 {

void SH101LFO::prepare(double sampleRate) {
    sr_ = sampleRate > 0.0 ? sampleRate : 48000.0;
    reset();
    setRate(rate_);
}

void SH101LFO::reset(uint32_t seed) {
    rng_.reset(seed);
    phase_ = 0.0;
    triangle_ = -1.0;
    square_ = -1.0;
    squareRise_ = false;
    shValue_ = 0.0;
    noiseState_ = 0.0;
    value_ = 0.0;
}

void SH101LFO::setRate(double hz) {
    rate_ = clampd(hz, 0.1, 30.0);
    // "Noise" modulator smoothing: corner scales with the rate so the wandering
    // speeds up with the knob, as a modulator would.  [APPROX]
    const double corner = clampd(rate_ * 8.0, 0.5, 400.0);
    noiseCoef_ = std::exp(-kTwoPi * corner / sr_);
}

void SH101LFO::setWave(int wave) {
    wave_ = clampi(wave, 0, 3);
}

void SH101LFO::process() {
    squareRise_ = false;

    const double inc = rate_ / sr_;
    phase_ += inc;
    if (phase_ >= 1.0) {
        phase_ -= 1.0;
        squareRise_ = true;                 // rising edge at each cycle start
        shValue_ = rng_.nextBipolar();      // random: latch a new step
    }
    phase_ = phase_ - std::floor(phase_);

    // Triangle: rises over the first half cycle, falls over the second.
    triangle_ = 1.0 - 4.0 * std::fabs(phase_ - 0.5);
    square_ = (phase_ < 0.5) ? 1.0 : -1.0;

    switch (wave_) {
        case Triangle: value_ = triangle_; break;
        case Square:   value_ = square_;   break;
        case Random:   value_ = shValue_;  break;  // stepped, held per cycle
        case Noise: {
            const double white = rng_.nextBipolar();
            noiseState_ = white * (1.0 - noiseCoef_) + noiseState_ * noiseCoef_;
            noiseState_ = flushDenormal(noiseState_);
            // Normalise the low-passed noise back to roughly +/-1.
            const double gain = 1.0 / std::max(0.05, std::sqrt(1.0 - noiseCoef_ * noiseCoef_));
            value_ = clampd(noiseState_ * gain * 0.35, -1.0, 1.0);
            break;
        }
        default: value_ = triangle_; break;
    }
}

} // namespace sh101
