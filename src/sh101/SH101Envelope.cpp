#include "sh101/SH101Envelope.h"

namespace sh101 {

void SH101Envelope::prepare(double sampleRate, const Calibration& cal) {
    sr_ = sampleRate > 0.0 ? sampleRate : 48000.0;
    cal_ = cal;
    attackTarget_ = cal_.envAttackTarget;
    updateCoefficients();
    reset();
}

void SH101Envelope::reset() {
    value_ = 0.0;
    stage_ = Idle;
    held_ = false;
    modGate_ = false;
    gate_ = false;
}

void SH101Envelope::setAttack(double seconds) { attackTime_ = clampd(seconds, 0.0005, 30.0); updateCoefficients(); }
void SH101Envelope::setDecay(double seconds)  { decayTime_ = clampd(seconds, 0.0005, 60.0); updateCoefficients(); }
void SH101Envelope::setRelease(double seconds){ releaseTime_ = clampd(seconds, 0.0005, 60.0); updateCoefficients(); }
void SH101Envelope::setSustain(double level01) { sustain_ = clampd(level01, 0.0, 1.0); }

void SH101Envelope::setTimeScale(double scale) {
    // Clamped to a sane band: a per-note RC tolerance is a few percent, never a
    // different envelope.
    const double clamped = clampd(scale, 0.90, 1.10);
    if (clamped != timeScale_) {
        timeScale_ = clamped;
        updateCoefficients();
    }
}

void SH101Envelope::updateCoefficients() {
    // Attack charges towards a target above the threshold, and the time constant
    // is chosen so the curve reaches 1.0 in exactly the parameterised time:
    //     v(t) = Vt * (1 - e^(-t/tau)) = 1   ->   tau = a / ln(Vt/(Vt-1))
    const double vt = (attackTarget_ > 1.000001) ? attackTarget_ : 1.000001;
    const double attackDivisor = std::log(vt / (vt - 1.0));
    const double settle = (cal_.envSettleRatio > 0.0) ? cal_.envSettleRatio : 4.605170186;
    coefA_ = segmentCoefficient(attackTime_ * timeScale_, attackDivisor);
    coefD_ = segmentCoefficient(decayTime_ * timeScale_, settle);
    coefR_ = segmentCoefficient(releaseTime_ * timeScale_, settle);
}

// Segment coefficient for a parameter time that means "the segment covers its
// excursion in this time", using the divisor appropriate to the segment:
// ln(Vt/(Vt-1)) for the attack's charge-towards-target curve, ln(100) for the
// exponential decay/release discharges (Calibration::envSettleRatio).
double SH101Envelope::segmentCoefficient(double seconds, double divisor) const {
    const double d = (divisor > 1.0e-6) ? divisor : 1.0e-6;
    const double tau = std::max(seconds, 1.0e-6) / d;
    const double n = tau * sr_;
    if (n < 1.0e-9) return 0.0;
    return std::exp(-1.0 / n);
}

void SH101Envelope::startAttack() {
    // Attack always begins from the current value, matching an RC network that
    // is re-charged rather than reset (this is the audible SH-101 retrigger
    // behaviour).
    stage_ = Attack;
}

void SH101Envelope::noteOn(bool retrigger) {
    held_ = true;
    if (triggerMode_ == LfoTrigger) {
        // In LFO mode the envelope gate is produced by the modulator; a key
        // press only arms it.
        gate_ = false;
        return;
    }
    gate_ = true;
    if (retrigger || (stage_ != Attack && stage_ != Decay && stage_ != Sustain)) {
        startAttack();
    }
}

void SH101Envelope::noteOff() {
    held_ = false;
    if (triggerMode_ != LfoTrigger) {
        gate_ = false;
        if (stage_ != Idle) stage_ = Release;
    }
}

void SH101Envelope::setModGate(bool high) {
    modGate_ = high;
}

double SH101Envelope::process() {
    if (triggerMode_ == LfoTrigger) {
        // Effective gate = key held AND modulator high; the rising edge of that
        // combination retriggers the envelope, which is the documented LFO
        // trigger mode behaviour.
        const bool eff = held_ && modGate_;
        if (eff && !gate_) {
            gate_ = true;
            startAttack();
        } else if (!eff && gate_) {
            gate_ = false;
            if (stage_ != Idle) stage_ = Release;
        }
    }

    switch (stage_) {
        case Attack: {
            const double target = attackTarget_;
            value_ = target + (value_ - target) * coefA_;
            if (value_ >= 1.0 || coefA_ == 0.0) {
                value_ = 1.0;
                stage_ = Decay;
            }
            break;
        }
        case Decay: {
            const double excursion = value_ - sustain_;
            if (excursion <= 1.0e-9) {
                value_ = sustain_;
                stage_ = Sustain;
                break;
            }
            value_ = sustain_ + excursion * coefD_;
            // Segment completes when 1% of the decay excursion is left, i.e.
            // after the parameterised decay time.
            if (value_ - sustain_ <= 0.01 * (1.0 - sustain_)) {
                value_ = sustain_;
                stage_ = Sustain;
            }
            break;
        }
        case Sustain:
            // Track the sustain slider; the parameter itself is smoothed by the
            // engine, so this stays click-free.
            value_ = sustain_;
            break;
        case Release: {
            value_ = value_ * coefR_;
            if (value_ <= 0.01 || coefR_ == 0.0) {
                value_ = 0.0;
                stage_ = Idle;
            }
            break;
        }
        case Idle:
        default:
            value_ = 0.0;
            break;
    }

    value_ = flushDenormal(value_);
    return value_;
}

} // namespace sh101
