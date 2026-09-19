// One-pole parameter smoothing (brief: "parameter smoothing for all
// audio-rate-sensitive controls", "no heap allocations in audio callback").
#pragma once

#include "sh101/Constants.h"

namespace sh101 {

class Smoother {
public:
    void prepare(double sampleRate) {
        sr_ = sampleRate > 0.0 ? sampleRate : 48000.0;
        setTimeConstantMs(5.0);
        snap(0.0);
    }
    void setTimeConstantMs(double ms) {
        tauSamples_ = std::max(1.0, (ms * 0.001) * sr_);
        coef_ = std::exp(-1.0 / tauSamples_);
    }
    void snap(double v) { value_ = target_ = v; }
    void setTarget(double v) { target_ = v; }
    double target() const { return target_; }
    double value() const { return value_; }
    double process() {
        value_ = target_ + (value_ - target_) * coef_;
        value_ = flushDenormal(value_);
        return value_;
    }
    bool settled() const { return std::fabs(value_ - target_) < 1.0e-9; }
    void setCurrent(double v) { value_ = v; }
private:
    double sr_ = 48000.0;
    double tauSamples_ = 240.0;
    double coef_ = 0.9958;
    double value_ = 0.0;
    double target_ = 0.0;
};

// Linear-interpolated per-block parameter ramp for control values that the
// voice reads once per sample.
class Ramp {
public:
    void prepare(double sampleRate, double ms = 10.0) {
        sr_ = sampleRate > 0.0 ? sampleRate : 48000.0;
        len_ = std::max(1.0, ms * 0.001 * sr_);
        inc_ = 0.0;
        set(0.0);
    }
    void set(double v) { from_ = to_ = v; value_ = v; inc_ = 0.0; }
    void moveTo(double v) {
        from_ = value_;
        to_ = v;
        inc_ = (to_ - from_) / len_;
    }
    double process() {
        value_ += inc_;
        if ((inc_ >= 0.0 && value_ >= to_) || (inc_ < 0.0 && value_ <= to_)) {
            value_ = to_;
            inc_ = 0.0;
        }
        return value_;
    }
    double value() const { return value_; }
private:
    double sr_ = 48000.0;
    double len_ = 480.0;
    double from_ = 0.0, to_ = 0.0, value_ = 0.0, inc_ = 0.0;
};

} // namespace sh101
