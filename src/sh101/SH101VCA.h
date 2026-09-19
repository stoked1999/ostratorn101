// SH101VCA — BA662A-based VCA.
//
// Hardware basis (brief, "VCA"): the VCA is a selected/offset-trimmed BA662A
// (an OTA), with two modes:
//   * ENV  — amplitude follows the ADSR,
//   * GATE — fixed level while the gate is high.
//
// Control law first, colour second: the OTA's gain is proportional to its
// control current, and the envelope/CV path converts voltage to that current
// through a resistive V-I stage, so to first order the gain is linear in the
// control voltage.  [APPROX] The exact taper of the SH-101's V-to-I stage is
// not transcribed from the schematic; it is a milestone-2/3 fitting target.
// The mild input compression of the OTA is modelled, since the brief notes that
// the interaction of filter output level, resonance and the BA662 path affects
// perceived character.
#pragma once

#include "sh101/Calibration.h"
#include "sh101/Constants.h"

namespace sh101 {

class SH101VCA {
public:
    enum Mode { Env = 0, Gate = 1 };

    void prepare(double sampleRate, const Calibration& cal = Calibration{});
    void setCalibration(const Calibration& cal) { cal_ = cal; updateShape(); }
    void reset();

    void setMode(int mode) { mode_ = clampi(mode, 0, 1); }
    void setGate(bool on) { gate_ = on; }

    double process(double x, double envValue);

    double lastGain() const { return lastGain_; }
    double lastControl() const { return lastControl_; }

private:
    void updateShape();

    int    mode_ = Env;
    bool   gate_ = false;
    double gainScale_ = 1.0;
    double nl_ = 0.2;
    double compHeadroom_ = 5.0;
    double leakage_ = 0.0;
    double lastGain_ = 0.0;
    double lastControl_ = 0.0;
    Calibration cal_{};
};

} // namespace sh101
