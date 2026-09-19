#include "sh101/StepSequencer.h"

namespace sh101 {

void StepSequencer::prepare(double sampleRate) {
    sr_ = sampleRate > 0.0 ? sampleRate : 48000.0;
    reset();
}

void StepSequencer::reset() {
    // Note: reset() deliberately does NOT wipe the programmed steps — a host
    // reset or panic must not erase the user's sequence.  Use clear() for that.
    phase_ = 0.0;
    index_ = 0;
    currentNote_ = -1;
    gate_ = false;
    externalTick_ = false;
    started_ = false;
}

void StepSequencer::clear() {
    for (int i = 0; i < kMaxSteps; ++i) {
        steps_[i].note = 60;
        steps_[i].gate = true;
        steps_[i].tie = false;
    }
}

void StepSequencer::setStep(int index, int note, bool gate, bool tie) {
    if (index < 0 || index >= kMaxSteps) return;
    steps_[index].note = clampi(note, 0, 127);
    steps_[index].gate = gate;
    steps_[index].tie = tie;
}

const StepSequencer::Step& StepSequencer::step(int index) const {
    const int i = clampi(index, 0, kMaxSteps - 1);
    return steps_[i];
}

int StepSequencer::effectiveNote(int index) const {
    return clampi(steps_[clampi(index, 0, kMaxSteps - 1)].note + transpose_, 0, 127);
}

void StepSequencer::stepTo(int index) {
    index_ = ((index % length_) + length_) % length_;
    currentNote_ = effectiveNote(index_);
}

StepSequencer::Event StepSequencer::process() {
    Event ev;
    if (!enabled_) {
        if (gate_) {
            gate_ = false;
            ev.gateChange = true;
            ev.gate = false;
        }
        phase_ = 0.0;
        return ev;
    }

    bool stepBoundary = false;
    if (externalClock_) {
        if (externalTick_) {
            externalTick_ = false;
            stepBoundary = true;
        }
    } else {
        phase_ += rate_ / sr_;
        if (phase_ >= 1.0) {
            phase_ -= std::floor(phase_);
            stepBoundary = true;
        }
    }

    const bool previousTie = gate_ && steps_[index_].tie;

    if (stepBoundary) {
        const int prevIndex = index_;
        // The first clock after (re)start plays step 0; later clocks advance.
        if (!started_) {
            started_ = true;
            stepTo(0);
        } else {
            stepTo(index_ + 1);
        }
        ev.noteChange = (currentNote_ != effectiveNote(prevIndex)) || !previousTie;
        ev.note = currentNote_;

        const Step& s = steps_[index_];
        if (!s.gate) {
            // Rest: gate stays low for this step.
            if (gate_) {
                gate_ = false;
                ev.gateChange = true;
                ev.gate = false;
            }
        } else if (!gate_) {
            gate_ = true;
            ev.gateChange = true;
            ev.gate = true;
        }
    }

    // Gate falls part way through the step unless it is tied into the next one.
    if (!externalClock_ && gate_ && !steps_[index_].tie && phase_ >= gateLength_) {
        gate_ = false;
        ev.gateChange = true;
        ev.gate = false;
    }

    return ev;
}

} // namespace sh101
