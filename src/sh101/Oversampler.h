// Oversampler2x — 2x interpolate / decimate around the nonlinear filter path.
//
// The brief asks for oversampling of the nonlinear VCF/VCA path (initially 2x
// or 4x) and to benchmark quality against CPU.  4x is two cascaded stages.
//
// Implementation: zero-stuffing interpolation and decimation through one
// windowed-sinc FIR (Blackman, L taps, cutoff just above the base-rate
// Nyquist).  Polyphase form is used on the way up so the work is 2 FIR passes
// per base sample; no allocations happen in process().
#pragma once

#include "sh101/Constants.h"

namespace sh101 {

class Oversampler2x {
public:
    static constexpr int kMaxTaps = 48;

    void prepare(int tapsPerPhase = 16) {
        P_ = clampi(tapsPerPhase, 8, kMaxTaps / 2);
        L_ = P_ * 2;
        // Cutoff slightly above the base-rate Nyquist (0.25 cycles/sample in the
        // 2x domain) to trade a little stopband margin for less passband droop.
        const double fcNorm = 0.28;
        const double m = 0.5 * (L_ - 1);
        double sum = 0.0;
        for (int n = 0; n < L_; ++n) {
            const double x = static_cast<double>(n) - m;
            const double sincArg = kTwoPi * fcNorm * x;
            const double sinc = (std::fabs(x) < 1e-12) ? 1.0 : std::sin(sincArg) / sincArg;
            // Blackman window
            const double w = 0.42 - 0.5 * std::cos(kTwoPi * n / (L_ - 1))
                                   + 0.08 * std::cos(2.0 * kTwoPi * n / (L_ - 1));
            h_[n] = 2.0 * fcNorm * sinc * w;
            sum += h_[n];
        }
        // Normalise to unity DC gain (the zero-stuffing compensation factor of
        // two is applied explicitly in the up path).
        if (std::fabs(sum) > 1e-12) {
            for (int n = 0; n < L_; ++n) h_[n] /= sum;
        }
        reset();
    }

    void reset() {
        for (int i = 0; i < kMaxTaps; ++i) inRing_[i] = 0.0;
        for (int i = 0; i < 2 * kMaxTaps; ++i) outRing_[i] = 0.0;
        inPos_ = 0;
        outPos_ = 0;
    }

    // Latency introduced by up+down filtering, expressed in base-rate samples.
    int latencySamples() const { return L_ / 2; }

    // Process one base-rate sample through a 2x-rate kernel.
    template <class Kernel>
    double process(double x, Kernel&& kernel) {
        // ---- Interpolation (zero-stuffing, polyphase) ----
        inRing_[inPos_] = x;
        double v0 = 0.0, v1 = 0.0;
        for (int i = 0; i < P_; ++i) {
            const double d = inRing_[(inPos_ - i + P_) % P_];
            v0 += h_[2 * i] * d;
            v1 += h_[2 * i + 1] * d;
        }
        inPos_ = (inPos_ + 1) % P_;
        v0 *= 2.0; // compensate the zero-stuffing amplitude loss
        v1 *= 2.0;

        // ---- Nonlinear kernel at 2x rate ----
        const double w0 = kernel(v0);
        const double w1 = kernel(v1);

        // ---- Decimation ----
        outRing_[outPos_] = w0;
        double y = 0.0;
        for (int k = 0; k < L_; ++k) {
            y += h_[k] * outRing_[(outPos_ - k + 2 * kMaxTaps) % (2 * kMaxTaps)];
        }
        outPos_ = (outPos_ + 1) % (2 * kMaxTaps);
        outRing_[outPos_] = w1;
        outPos_ = (outPos_ + 1) % (2 * kMaxTaps);

        return y;
    }

private:
    int P_ = 16;
    int L_ = 32;
    double h_[kMaxTaps] = {};
    double inRing_[kMaxTaps] = {};
    double outRing_[2 * kMaxTaps] = {};
    int inPos_ = 0;
    int outPos_ = 0;
};

} // namespace sh101
