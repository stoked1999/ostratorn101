#include "sh101/SH101VCA.h"

namespace sh101 {

void SH101VCA::prepare(double sampleRate, const Calibration& cal) {
    (void)sampleRate;
    cal_ = cal;
    updateShape();
    reset();
}

void SH101VCA::updateShape() {
    gainScale_ = cal_.vcaGainScale;
    nl_ = clampd(cal_.vcaNonlinearity, 0.0, 1.0);
    // nl = 0 -> ideal OTA (linear); larger nl -> earlier compression.
    compHeadroom_ = (nl_ > 1.0e-6) ? (1.0 / nl_) : 1.0e6;
    leakage_ = cal_.vcaOffLeakage;
}

void SH101VCA::reset() {
    lastGain_ = 0.0;
    lastControl_ = 0.0;
}

double SH101VCA::process(double x, double envValue) {
    double ctrl = (mode_ == Gate) ? (gate_ ? 1.0 : 0.0) : clampd(envValue, 0.0, 1.0);
    lastControl_ = ctrl;

    const double gain = ctrl * gainScale_;
    lastGain_ = gain;

    // OTA input compression: y = gain * H*x where H is the differential pair's
    // tanh transfer, plus a small closed-VCA feedthrough term.
    const double xs = compHeadroom_ * std::tanh(x / compHeadroom_);
    const double y = gain * xs + leakage_ * x;

    return flushDenormal(y);
}

} // namespace sh101
