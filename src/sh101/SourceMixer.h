// SourceMixer — four-source mixer feeding the VCF.
//
// Hardware basis (brief, "Source mixer"): pulse/square, sawtooth, sub
// oscillator and noise are summed into the filter.  The brief is explicit that
// four digital gains summed at unity will not reproduce the hardware, and that
// the level into the IR3109 matters because its nonlinear behaviour changes
// with drive.  This block therefore:
//
//   1. applies per-source gains from the calibration set (mixer gain staging),
//   2. sums them,
//   3. runs the sum through an input-stage soft clipper whose knee is the
//      "mixerHeadroom" calibration constant — the modelled drive into the
//      IR3109 input amplifier.
//
// [APPROX] Gains and headroom are not yet fitted to measurements; they are the
// milestone-2 target ("match source-mixer gain staging").  The pre-clip sum is
// exposed so tests can assert the gain structure independently of the clipper.
#pragma once

#include "sh101/Calibration.h"
#include "sh101/Constants.h"

namespace sh101 {

class SourceMixer {
public:
    void prepare(double sampleRate, const Calibration& cal = Calibration{});
    void setCalibration(const Calibration& cal) { cal_ = cal; }
    void reset();

    // Levels are the front-panel mixer levels, 0..1.
    void setLevels(double sawLevel, double pulseLevel, double subLevel, double noiseLevel);

    // One sample: bipolar source signals in [-1, 1].
    double process(double saw, double pulse, double sub, double noise);

    double lastSum() const { return lastSum_; }       // pre-clip sum (tests)
    double lastOutput() const { return lastOutput_; } // post-clip
    double lastDrive() const { return lastSum_; }

private:
    double sawGain_ = 0.0, pulseGain_ = 0.0, subGain_ = 0.0, noiseGain_ = 0.0;
    double lastSum_ = 0.0, lastOutput_ = 0.0;
    Calibration cal_{};
};

} // namespace sh101
