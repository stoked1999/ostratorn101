#include "sh101/OutputStage.h"

namespace sh101 {

void OutputStage::prepare(double sampleRate, const Calibration& cal) {
    sr_ = sampleRate > 0.0 ? sampleRate : 48000.0;
    cal_ = cal;
    updateFilters();
    reset();
}

void OutputStage::updateFilters() {
    const double fc = clampd(cal_.outputDcBlockHz, 0.1, sr_ * 0.4);
    dcCoef_ = std::exp(-kTwoPi * fc / sr_);
}

void OutputStage::reset() {
    dcX1_ = 0.0;
    dcY1_ = 0.0;
}

double OutputStage::process(double x) {
    const double g = volume_ * cal_.outputGain;
    const double hr = cal_.outputHeadroom > 0.0 ? cal_.outputHeadroom : 1.0;

    // Output amplifier with finite headroom (soft limiting, no hard clipping so
    // the model never produces discontinuities of its own).
    const double amplified = hr * std::tanh((x * g) / hr);

    // Capacitor coupling / DC blocking.
    const double y = dcCoef_ * (dcY1_ + amplified - dcX1_);
    dcX1_ = amplified;
    dcY1_ = flushDenormal(y);
    return dcY1_;
}

} // namespace sh101
