// SH101VCO — CEM3340-inspired oscillator.
//
// Hardware basis (brief, "VCO"): the oscillator core is a CEM3340.  The SH-101
// exposes the saw and pulse outputs through the source mixer; the CEM3340 also
// has a triangle output, which this model keeps available because the sub
// oscillator divider is clocked from the oscillator's core square/edge stream.
//
// Implementation notes:
//   * Band-limited generation (polyBLEP) is used for every output — the brief
//     explicitly forbids naive discontinuous saw/pulse generation.
//   * Pitch is carried as a 1 V/oct CV and converted to Hz once per sample,
//     keeping all modulation in the CV domain.
//   * Range switching (16'/8'/4'/2') is an octave offset plus the per-range
//     "range width" service trim.
#pragma once

#include "sh101/Calibration.h"
#include "sh101/Constants.h"

namespace sh101 {

class SH101VCO {
public:
    void prepare(double sampleRate, const Calibration& cal = Calibration{});
    void setCalibration(const Calibration& cal) { cal_ = cal; updateFrequency(); }
    void reset(double phase = 0.0);

    // 0 = 16', 1 = 8', 2 = 4', 3 = 2'  (service: "range width" trims each one)
    void setRange(int rangeIndex);
    void setFineTuneCents(double cents);   // +/-50 cents front-panel tune
    void setPulseWidth(double width);      // 0.03 .. 0.97, clamped
    void setPitchCV(double cv);            // volts, 1 V/oct, 0 V = C4
    void setAnalogDrift(bool enabled, double centsDepth = 3.0, uint32_t seed = 0x5eedu);

    void process();                        // advance one sample

    // Waveform taps (valid after process()).
    double saw() const        { return saw_; }
    double pulse() const      { return pulse_; }
    double coreSquare() const { return core_; }
    double phase() const      { return phase_; }
    bool   wrapped() const    { return wrapped_; }
    double frequency() const  { return hz_; }

private:
    void updateFrequency();

    double sr_ = 48000.0;
    double phase_ = 0.0;
    double hz_ = kRefFreqC4;
    double pw_ = 0.5;
    double cv_ = 0.0;
    int    range_ = 1;
    double fineCents_ = 0.0;
    bool   wrapped_ = false;

    double saw_ = 0.0, pulse_ = 0.0, core_ = 0.0;

    bool   driftOn_ = false;
    double driftCents_ = 3.0;
    double driftValue_ = 0.0;
    XorShift32 driftRng_{0x5eedu};

    Calibration cal_{};
};

} // namespace sh101
