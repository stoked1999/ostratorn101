// SH101Envelope — the single ADSR of the SH-101.
//
// Hardware basis (brief, "Envelope generator"):
//   * one ADSR shared as a control source (filter cutoff, PWM, and the VCA in
//     ENV mode),
//   * documented ranges: A ~1.5 ms..4 s, D ~2 ms..10 s, S 0..100%,
//     R ~2 ms..10 s,
//   * trigger modes GATE+TRIG / GATE / LFO,
//   * the brief forbids "arbitrary linear ramps" and asks for the charging /
//     discharging network's analog curve shape and taper.
//
// Model: each segment is an RC charge/discharge towards a target, so the shape
// is exponential, as on the board:
//   * attack charges towards a target above the trigger threshold
//     (calibration: envAttackTarget, default 1.30) and stops at 1.0, which is
//     what gives the analog attack its characteristic slightly convex curve and
//     its fast, punchy onset for short times,
//   * decay discharges from the peak towards sustain, release from wherever the
//     envelope is towards zero.
//
// A parameter time is defined as the time to cover 99% of the segment's
// excursion (ln(100) time constants).  That definition makes the documented
// ranges directly measurable, which is what the validation tests assert.
//
// Sustained behaviour: in LFO trigger mode the effective gate follows the LFO
// square, so the envelope cycles while a key is held.
#pragma once

#include "sh101/Calibration.h"
#include "sh101/Constants.h"

namespace sh101 {

class SH101Envelope {
public:
    enum Stage { Idle = 0, Attack, Decay, Sustain, Release };
    enum TriggerMode { GateAndTrig = 0, GateOnly = 1, LfoTrigger = 2 };

    void prepare(double sampleRate, const Calibration& cal = Calibration{});
    void setCalibration(const Calibration& cal) { cal_ = cal; updateCoefficients(); }
    void reset();

    void setAttack(double seconds);
    void setDecay(double seconds);
    void setSustain(double level01);
    void setRelease(double seconds);
    void setTriggerMode(int mode) { triggerMode_ = clampi(mode, 0, 2); }

    // Per-note RC tolerance (AnalogVariation): multiplies the segment times of
    // the note in progress.  attackTime()/decayTime()/releaseTime() keep
    // reporting the *parameterised* times, which is what the documented ranges
    // and their tests refer to.
    void setTimeScale(double scale);
    double timeScale() const { return timeScale_; }

    double attackTime() const { return attackTime_; }
    double decayTime() const { return decayTime_; }
    double releaseTime() const { return releaseTime_; }

    // Gate handling.  retrigger = true forces a new attack (GATE+TRIG mode, or
    // any retrigger the engine decides on); false only starts an attack when the
    // envelope is not already running (GATE mode, legato).
    void noteOn(bool retrigger);
    void noteOff();
    void setModGate(bool high);   // LFO square state (LFO trigger mode only)

    double process();
    double value() const { return value_; }
    int    stage() const { return stage_; }
    bool   gateActive() const { return gate_; }
    bool   isRunning() const { return stage_ != Idle; }

private:
    void updateCoefficients();
    double segmentCoefficient(double seconds, double divisor) const;
    void startAttack();

    double sr_ = 48000.0;
    double attackTime_ = 0.005;
    double decayTime_ = 0.30;
    double releaseTime_ = 0.30;
    double sustain_ = 0.7;
    int    triggerMode_ = GateAndTrig;
    double timeScale_ = 1.0;   // per-note RC tolerance multiplier

    double coefA_ = 0.0, coefD_ = 0.0, coefR_ = 0.0;
    double attackTarget_ = 1.30;

    double value_ = 0.0;
    int    stage_ = Idle;

    bool   held_ = false;      // keyboard gate
    bool   modGate_ = false;   // LFO square (LFO trigger mode)
    bool   gate_ = false;      // effective gate after trigger-mode combination
    Calibration cal_{};
};

} // namespace sh101
