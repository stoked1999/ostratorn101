#include "sh101/SubOscDivider.h"

namespace sh101 {

void SubOscDivider::prepare(double sampleRate) {
    sr_ = sampleRate > 0.0 ? sampleRate : 48000.0;
    reset();
}

void SubOscDivider::reset() {
    // The service schematic clears the divider flip-flops with the gate; the
    // sub therefore always starts from the same state on a new note.
    cycle_ = 0;
    value_ = -1.0;
    na_ = -1.0;
}

void SubOscDivider::setMode(int mode) {
    mode_ = clampi(mode, 0, 2);
    N_ = (mode_ == Down1Square) ? 2 : 4;
    duty_ = (mode_ == Down2NarrowPulse) ? 0.25 : 0.5;
}

void SubOscDivider::process(double masterPhase, bool masterWrapped, double masterDt) {
    // Divider counter: increments on every master rising edge, wraps at N.
    if (masterWrapped) {
        cycle_ = (cycle_ + 1) % N_;
    }

    // Flip-flop chain output phase: continuous ramp, slope = masterDt / N.
    double q = (masterPhase + static_cast<double>(cycle_)) / static_cast<double>(N_);
    q -= std::floor(q);

    // Square/pulse with the divider's duty cycle, band-limited with the sub's
    // own increment (masterDt / N).
    const double dtSub = masterDt / static_cast<double>(N_);
    const double naive = (q < duty_) ? 1.0 : -1.0;
    double q2 = q - duty_;
    if (q2 < 0.0) q2 += 1.0;
    double out = naive + polyBlep(q, dtSub) - polyBlep(q2, dtSub);

    if (coreInverted_) out = -out;
    value_ = out;
    na_ = naive;
}

} // namespace sh101
