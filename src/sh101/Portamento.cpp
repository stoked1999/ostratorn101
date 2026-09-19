#include "sh101/Portamento.h"

namespace sh101 {

void Portamento::prepare(double sampleRate, const Calibration& cal) {
    sr_ = sampleRate > 0.0 ? sampleRate : 48000.0;
    settleRatio_ = cal.envSettleRatio > 0.0 ? cal.envSettleRatio : 4.605170186;
    setTime(time_);
    reset(0.0);
}

void Portamento::reset(double cv) {
    value_ = cv;
    gliding_ = false;
}

void Portamento::setTime(double seconds) {
    time_ = clampd(seconds, 0.0, 5.0);
    const double tau = time_ / settleRatio_;
    const double n = tau * sr_;
    coef_ = (n < 1.0e-9) ? 0.0 : std::exp(-1.0 / n);
}

void Portamento::setMode(int mode) {
    mode_ = clampi(mode, 0, 2);
}

double Portamento::process(double targetCV) {
    const bool glideActive = (mode_ == On) || (mode_ == Auto && autoEligible_);
    if (!glideActive || time_ <= 0.0 || coef_ == 0.0) {
        value_ = targetCV;
        gliding_ = false;
        return value_;
    }
    const double diff = targetCV - value_;
    if (diff == 0.0) {
        gliding_ = false;
        return value_;
    }
    value_ = targetCV + (value_ - targetCV) * coef_;
    // Snap once the remaining interval is inaudible so pitch tracking tests and
    // steady-state tuning are exact.
    if (std::fabs(targetCV - value_) < 1.0e-7) {
        value_ = targetCV;
        gliding_ = false;
    } else {
        gliding_ = true;
    }
    return value_;
}

} // namespace sh101
