// SH101VCF — four-stage OTA (IR3109) low-pass filter.
//
// Hardware basis (brief, "VCF — highest-priority authenticity block"):
//   * filter IC is the Roland IR3109: four OTA-based filter stages plus buffers
//     and exponential cutoff-control circuitry, used as a 4-pole low-pass,
//   * resonance runs from zero through self-oscillation,
//   * the service calibration procedure tunes VCF tracking while the filter is
//     resonating/self-oscillating, so resonance and cutoff CV tracking are part
//     of the intended design,
//   * the brief forbids substituting a generic biquad cascade, a generic ladder
//     filter, or a Juno implementation (different surrounding feedback/gain).
//
// Model: the four stages are OTA integrators, i.e. one-pole low-passes with a
// transconductance-controlled cutoff, coupled by a feedback path from the last
// stage back to the first (SH-101-specific feedback/gain network).  The
// topology-preserving (zero-delay-feedback) transform is used so the loop is
// solved implicitly and stays stable at maximum resonance.
//
// Nonlinearity: OTA gain cells compress, so the input stage and the feedback
// node are soft-limited, and the integrator states are limited to a finite
// headroom.  That is what bounds self-oscillation amplitude.
//
// [APPROX] The stage ordering, feedback network coefficients and saturation
// constants are not yet transcribed from the schematic value-by-value, and the
// resonance make-up (bootstrapping/bass-loss) of the real board is not modelled
// beyond the feedback coefficient.  These are the milestone-2/3 calibration
// targets; every such spot is marked [APPROX].
//
// Not done here (deliberately): cutoff CV generation.  The engine computes
// cutoff in volts (manual + envelope + LFO + key follow + bender) and hands the
// filter a frequency, because the service notes scale cutoff CV with "VCF
// width"/"VCF tracking" trims that belong to the CV path.
#pragma once

#include "sh101/Calibration.h"
#include "sh101/Constants.h"
#include "sh101/Oversampler.h"

namespace sh101 {

class SH101VCF {
public:
    void prepare(double sampleRate, const Calibration& cal = Calibration{}, int oversampleFactor = 1);
    void setCalibration(const Calibration& cal) { cal_ = cal; updateCoefficients(); }
    void reset();

    void setCutoffHz(double hz);
    void setResonance(double r01);          // 0..1, 1.0 reaches self-oscillation
    void setInputDrive(double drive);       // 1.0 = nominal
    void setOversampleFactor(int f);        // 1, 2 or 4

    double process(double x);

    double cutoffHz() const { return cutoffHz_; }
    double feedbackK() const { return k_; }
    int    oversampleFactor() const { return factor_; }
    // Self-oscillation is reached when the loop gain k * (1/4) exceeds unity;
    // exposed for tests and calibration.
    double loopGain() const { return k_ * 0.25; }
    bool   selfOscillating() const { return loopGain() > 1.0; }

private:
    void updateCoefficients();
    double core(double x);

    double sr_ = 48000.0;
    double srOs_ = 48000.0;
    double g_ = 0.0;      // tan(pi * fc / fs_os)
    double G_ = 0.0;      // g / (1 + g): one-pole TPT coefficient
    double k_ = 0.0;      // feedback coefficient
    double drive_ = 1.0;
    double cutoffHz_ = 1200.0;
    double s_[4] = { 0.0, 0.0, 0.0, 0.0 };
    int    factor_ = 1;
    Calibration cal_{};
    Oversampler2x os1_;
    Oversampler2x os2_;
};

} // namespace sh101
