#include "sh101/SH101VCF.h"

namespace sh101 {

namespace {
// OTA headroom constants.  [APPROX] These set how hard the stages compress;
// milestone-2 replaces them with values fitted to measured filter sweeps.
constexpr double kInputHeadroom   = 2.0;
constexpr double kFeedbackHeadroom = 1.2;
constexpr double kStateHeadroom   = 1.6;

inline double softLimit(double x, double hr) {
    return hr * std::tanh(x / hr);
}
}

void SH101VCF::prepare(double sampleRate, const Calibration& cal, int oversampleFactor) {
    sr_ = sampleRate > 0.0 ? sampleRate : 48000.0;
    cal_ = cal;
    os1_.prepare(16);
    os2_.prepare(16);
    setOversampleFactor(oversampleFactor);
    reset();
    updateCoefficients();
}

void SH101VCF::setOversampleFactor(int f) {
    factor_ = (f >= 4) ? 4 : (f == 2 ? 2 : 1);
    srOs_ = sr_ * factor_;
    os1_.reset();
    os2_.reset();
    updateCoefficients();
}

void SH101VCF::reset() {
    s_[0] = s_[1] = s_[2] = s_[3] = 0.0;
    os1_.reset();
    os2_.reset();
}

void SH101VCF::setCutoffHz(double hz) {
    // Documented cutoff range 10 Hz .. 20 kHz, limited below Nyquist.
    const double nyq = 0.45 * srOs_;
    cutoffHz_ = clampd(hz, cal_.filterFcMinHz, std::min(cal_.filterFcMaxHz, nyq));
    updateCoefficients();
}

void SH101VCF::setResonance(double r01) {
    // Resonance runs from zero through self-oscillation.  The loop gain of the
    // four-stage cascade at the corner frequency is k/4, so k must exceed 4 for
    // self-oscillation; the calibration constant sets where the slider tops out.
    k_ = clampd(r01, 0.0, 1.0) * cal_.resSelfOscK;
}

void SH101VCF::setInputDrive(double drive) {
    drive_ = clampd(drive, 0.0, 8.0);
}

void SH101VCF::updateCoefficients() {
    // OTA cutoff control is exponential in control current; the CV -> frequency
    // conversion happens in the engine, so here the coefficient is pre-warped
    // for the (possibly oversampled) rate.
    const double fc = clampd(cutoffHz_, cal_.filterFcMinHz, 0.45 * srOs_);
    g_ = std::tan(kPi * fc / srOs_);
    G_ = g_ / (1.0 + g_);
}

double SH101VCF::core(double x) {
    const double G = G_;
    const double oneMinusG = 1.0 - G;

    // Instantaneous (ZDF) response of the cascade: each stage is
    //   y_i = G * x_i + (1-G) * s_i
    // so y4 = A*u + B with A = G^4 and B the state-dependent part.
    const double b1 = oneMinusG * s_[0];
    const double b2 = oneMinusG * s_[1];
    const double b3 = oneMinusG * s_[2];
    const double b4 = oneMinusG * s_[3];
    const double G2 = G * G;
    const double A = G2 * G2;
    const double B = G2 * G * b1 + G2 * b2 + G * b3 + b4;

    // Input stage compression (OTA gain cell headroom).
    const double u = softLimit(x * drive_, kInputHeadroom);

    // Solve the feedback loop implicitly: y4 = A*(u - k*y4) + B.
    const double y4solved = (A * u + B) / (1.0 + A * k_);

    // Feedback node limiting bounds self-oscillation amplitude.
    const double fb = softLimit(y4solved, kFeedbackHeadroom);

    // Propagate with the limited feedback signal and update the integrator
    // states (TPT: s_new = 2*y - s_old).
    const double x1 = u - k_ * fb;
    const double y1 = G * x1 + b1;
    const double y2 = G * y1 + b2;
    const double y3 = G * y2 + b3;
    const double y4 = G * y3 + b4;

    s_[0] = flushDenormal(clampd(2.0 * y1 - s_[0], -kStateHeadroom * 4.0, kStateHeadroom * 4.0));
    s_[1] = flushDenormal(clampd(2.0 * y2 - s_[1], -kStateHeadroom * 4.0, kStateHeadroom * 4.0));
    s_[2] = flushDenormal(clampd(2.0 * y3 - s_[2], -kStateHeadroom * 4.0, kStateHeadroom * 4.0));
    s_[3] = flushDenormal(clampd(2.0 * y4 - s_[3], -kStateHeadroom * 4.0, kStateHeadroom * 4.0));

    return y4;
}

double SH101VCF::process(double x) {
    if (factor_ == 1) {
        return core(x);
    }
    if (factor_ == 2) {
        return os1_.process(x, [this](double v) { return core(v); });
    }
    // 4x: two cascaded 2x stages.
    return os1_.process(x, [this](double v) {
        return os2_.process(v, [this](double w) { return core(w); });
    });
}

} // namespace sh101
