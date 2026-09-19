// NoiseGenerator — transistor noise source (service parts list: selected
// 2SC945 in the noise generator, with IR9022 amplification/conditioning).
//
// Brief guidance honoured here:
//   * white noise with deterministic seeding for tests,
//   * an explicit conditioning stage rather than "assume ideal flat white
//     noise is the final sound".
//
// [APPROX] The 2SC945 generator's spectrum and the conditioning network's
// transfer are not derived from the schematic here; this is a white source
// through a one-pole high-pass shelf plus a level trim.  Fidelity milestone 3
// ("noise spectrum" measurement) is where these constants get fitted.
#pragma once

#include "sh101/Calibration.h"
#include "sh101/Constants.h"

namespace sh101 {

class NoiseGenerator {
public:
    void prepare(double sampleRate, const Calibration& cal = Calibration{});
    void setCalibration(const Calibration& cal) { cal_ = cal; updateFilters(); }
    void reset(uint32_t seed = 0x1234567u);

    // Advance one sample; returns conditioned noise in roughly [-1, 1].
    double process();

    // Unconditioned white sample (used by tests and by the "noise" LFO wave).
    double white() const { return white_; }
    // Conditioned sample (audio-path noise source).
    double conditioned() const { return conditioned_; }

private:
    void updateFilters();

    double sr_ = 48000.0;
    double hpCoef_ = 0.0;
    double hpState_ = 0.0;
    double hpInputPrev_ = 0.0;
    double white_ = 0.0;
    double conditioned_ = 0.0;
    Calibration cal_{};
    XorShift32 rng_{0x1234567u};
};

} // namespace sh101
