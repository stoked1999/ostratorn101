#include "sh101/SH101VCO.h"

namespace sh101 {

namespace {
// Range switch offsets in octaves relative to 8'.  16' is one octave below 8',
// 4' one octave above, 2' two octaves above.
constexpr double kRangeOctaves[4] = { -1.0, 0.0, 1.0, 2.0 };
}

void SH101VCO::prepare(double sampleRate, const Calibration& cal) {
    sr_ = sampleRate > 0.0 ? sampleRate : 48000.0;
    cal_ = cal;
    phase_ = 0.0;
    updateFrequency();
    saw_ = pulse_ = core_ = 0.0;
}

void SH101VCO::reset(double phase) {
    phase_ = phase - std::floor(phase);
    wrapped_ = false;
    driftValue_ = 0.0;
    driftRng_.reset(0x5eedu);
}

void SH101VCO::setRange(int rangeIndex) {
    range_ = clampi(rangeIndex, 0, 3);
    updateFrequency();
}

void SH101VCO::setFineTuneCents(double cents) {
    fineCents_ = clampd(cents, -100.0, 100.0);
    updateFrequency();
}

void SH101VCO::setPulseWidth(double width) {
    // The hardware pulse never fully collapses; keep the duty cycle in a safe,
    // non-zero band (brief: "pulse width stays within safe nonzero limits").
    pw_ = clampd(width, 0.03, 0.97);
}

void SH101VCO::setPitchCV(double cv) {
    cv_ = cv;
    updateFrequency();
}

void SH101VCO::setAnalogDrift(bool enabled, double centsDepth, uint32_t seed) {
    driftOn_ = enabled;
    driftCents_ = centsDepth;
    driftRng_.reset(seed);
    driftValue_ = 0.0;
}

void SH101VCO::updateFrequency() {
    // Service trims: "VCO tune" (static cents), "VCO width" (octave scaling of
    // the whole CV) and per-position "range width".
    const double trimmedCv = cv_ * cal_.vcoWidth * cal_.rangeWidth[range_];
    const double octaves = trimmedCv + kRangeOctaves[range_];
    const double cents = cal_.vcoTuneCents + fineCents_ + (driftOn_ ? driftValue_ : 0.0);
    const double f = cvToFrequency(octaves + cents / 1200.0);
    hz_ = clampd(f, 0.01, sr_ * 0.49);
}

void SH101VCO::process() {
    if (driftOn_) {
        // Slow random walk, a few samples at a time; purely cosmetic and off by
        // default (brief: add only after the deterministic model is validated).
        driftValue_ = flushDenormal(driftValue_ * 0.9995 + driftRng_.nextBipolar() * driftCents_ * 0.002);
        updateFrequency();
    }

    // Pulse width modulation is applied upstream (engine) into pw_; here we
    // only consume it.
    const double p = phase_;
    // The phase increment follows the *current* pitch CV: any pitch change
    // (keyboard, portamento, LFO, bender, tune) takes effect immediately.
    const double dt = clampd(hz_ / sr_, 0.0, 0.49);

    // --- Sawtooth: naive ramp minus the polyBLEP residual at the wrap --------
    saw_ = (2.0 * p - 1.0) - polyBlep(p, dt);

    // --- Pulse: two edges, one riser at phase 0 and one faller at phase pw ---
    double p2 = p - pw_;
    if (p2 < 0.0) p2 += 1.0;
    const double naivePulse = (p < pw_) ? 1.0 : -1.0;
    pulse_ = naivePulse + polyBlep(p, dt) - polyBlep(p2, dt);

    // --- Core square: the edge stream that clocks the flip-flop divider -------
    const double p3 = (p < 0.5) ? (p + 0.5) : (p - 0.5);
    const double naiveCore = (p < cal_.cemSquareDuty) ? 1.0 : -1.0;
    core_ = naiveCore + polyBlep(p, dt) - polyBlep(p3, dt);

    // Advance phase; record whether the cycle boundary was crossed this sample.
    phase_ += dt;
    wrapped_ = false;
    if (phase_ >= 1.0) {
        phase_ -= 1.0;
        wrapped_ = true;
    }
}

} // namespace sh101
