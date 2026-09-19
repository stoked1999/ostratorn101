// SH101LFO — the low-frequency modulator.
//
// Hardware basis (brief, "LFO / modulator"):
//   * rate range about 0.1 Hz .. 30 Hz,
//   * waveform selections: triangle, square, random, noise,
//   * it can modulate VCO pitch and VCF cutoff, and can be selected as the PWM
//     source,
//   * random and noise modulation are *distinct* behaviours and must not be
//     collapsed into one algorithm.
//
// Implementation:
//   * triangle / square are phase-locked to a common accumulator, so switching
//     waveform does not jump,
//   * random = sample-and-hold: a new random value is latched at each cycle
//     boundary (stepped),
//   * noise = continuous smooth random: white noise through a low-pass whose
//     corner scales with the LFO rate, so it wanders rather than steps.
#pragma once

#include "sh101/Constants.h"

namespace sh101 {

class SH101LFO {
public:
    enum Wave { Triangle = 0, Square = 1, Random = 2, Noise = 3 };

    void prepare(double sampleRate);
    void reset(uint32_t seed = 0x10f010f0u);

    void setRate(double hz);      // 0.1 .. 30 Hz
    void setWave(int wave);

    void process();

    double value() const { return value_; }               // bipolar modulation signal
    double triangle() const { return triangle_; }
    double unipolar() const { return 0.5 * (triangle_ + 1.0); } // 0..1, used for PWM
    double square() const { return square_; }
    bool   squareHigh() const { return square_ > 0.0; }
    bool   squareRisingEdge() const { return squareRise_; }
    double rateHz() const { return rate_; }
    double phase() const { return phase_; }

private:
    double sr_ = 48000.0;
    double rate_ = 5.0;
    int    wave_ = Triangle;
    double phase_ = 0.0;
    double triangle_ = -1.0;
    double square_ = -1.0;
    bool   squareRise_ = false;
    double shValue_ = 0.0;     // sample & hold (random) state
    double noiseState_ = 0.0;  // smoothed noise state
    double noiseCoef_ = 0.0;
    double value_ = 0.0;
    XorShift32 rng_{0x10f010f0u};
};

} // namespace sh101
