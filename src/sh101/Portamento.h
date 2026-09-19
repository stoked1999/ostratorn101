// Portamento — glide applied to the keyboard CV.
//
// Hardware basis (brief, "Portamento"): time range approximately 0..5 s, modes
// ON / OFF / AUTO.  AUTO applies glide "in the intended legato context rather
// than always gliding every note" — i.e. only when a new note is taken while
// another key is still held.
//
// The glide itself is an RC-style approach to the target CV, parameterised the
// same way as the envelope times (time to cover 99% of the interval), so the
// timing is measurable and matches the documented range.
#pragma once

#include "sh101/Calibration.h"
#include "sh101/Constants.h"

namespace sh101 {

class Portamento {
public:
    enum Mode { Off = 0, On = 1, Auto = 2 };

    void prepare(double sampleRate, const Calibration& cal = Calibration{});
    void reset(double cv = 0.0);

    void setTime(double seconds);        // 0..5 s
    void setMode(int mode);
    // In AUTO mode: true only when the note change happened legato.
    void setAutoEligible(bool eligible) { autoEligible_ = eligible; }

    double process(double targetCV);
    double value() const { return value_; }
    bool   gliding() const { return gliding_; }
    double time() const { return time_; }

private:
    double sr_ = 48000.0;
    double time_ = 0.0;
    int    mode_ = On;
    bool   autoEligible_ = false;
    double value_ = 0.0;
    double coef_ = 0.0;
    double settleRatio_ = 4.605170186;
    bool   gliding_ = false;
};

} // namespace sh101
