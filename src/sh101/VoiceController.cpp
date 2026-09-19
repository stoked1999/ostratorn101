#include "sh101/VoiceController.h"

namespace sh101 {

void VoiceController::reset() {
    count_ = 0;
    pedalCount_ = 0;
    pedalDown_ = false;
    gateEventHead_ = 0;
    gateEventCount_ = 0;
    gate_ = false;
    currentNote_ = -1;
    legatoEvent_ = false;
    triggerEvent_ = false;
    noteChanged_ = false;
    velocity_ = 1.0;
}

void VoiceController::pushGateEvent(bool isRise) {
    if (gateEventCount_ >= kMaxGateEvents) {
        // Queue full: the oldest transition is dropped rather than losing the
        // most recent one (the newest event is what the current state reflects).
        gateEventHead_ = (gateEventHead_ + 1) % kMaxGateEvents;
        --gateEventCount_;
    }
    const int index = (gateEventHead_ + gateEventCount_) % kMaxGateEvents;
    gateEventRise_[index] = isRise;
    ++gateEventCount_;
}

bool VoiceController::takeGateEvent(bool& isRise) {
    if (gateEventCount_ <= 0) return false;
    isRise = gateEventRise_[gateEventHead_];
    gateEventHead_ = (gateEventHead_ + 1) % kMaxGateEvents;
    --gateEventCount_;
    return true;
}

void VoiceController::setGate(bool on) {
    if (on == gate_) return;
    gate_ = on;
    pushGateEvent(on);
}

void VoiceController::noteOn(int midiNote, double velocity) {
    if (count_ < kMaxNotes) {
        // Last-note priority: the newest note goes on top of the stack.
        stack_[count_++] = midiNote;
    } else {
        // Stack overflow: drop the oldest held note (host protection).
        for (int i = 1; i < kMaxNotes; ++i) stack_[i - 1] = stack_[i];
        stack_[kMaxNotes - 1] = midiNote;
    }
    velocity_ = velocity;

    legatoEvent_ = gate_;      // a note arrived while the gate was already high
    triggerEvent_ = true;      // every note-on is a trigger event
    noteChanged_ = (currentNote_ != midiNote);
    setGate(true);
    currentNote_ = midiNote;
}

void VoiceController::noteOff(int midiNote) {
    bool removed = false;
    for (int i = count_ - 1; i >= 0; --i) {
        if (stack_[i] == midiNote) {
            for (int j = i; j < count_ - 1; ++j) stack_[j] = stack_[j + 1];
            --count_;
            removed = true;
            break;
        }
    }
    if (!removed) return;

    if (pedalDown_) {
        // Sustain pedal holds the note: remember it so the pedal's release can
        // end it later without disturbing the stack order.
        if (pedalCount_ < kMaxNotes) pedalStack_[pedalCount_++] = midiNote;
    }

    setGateFromState();
}

void VoiceController::allNotesOff() {
    count_ = 0;
    pedalCount_ = 0;
    setGate(false);
    currentNote_ = -1;
    triggerEvent_ = false;
    legatoEvent_ = false;
    noteChanged_ = true;
}

void VoiceController::setSustainPedal(bool down) {
    if (down == pedalDown_) return;
    pedalDown_ = down;
    if (!down) {
        // Pedal released: the deferred notes end now.
        pedalCount_ = 0;
        setGateFromState();
        noteChanged_ = true;
    }
}

void VoiceController::setGateFromState() {
    const int newTop = topOfStack();
    if (newTop < 0) {
        if (pedalDown_ && pedalCount_ > 0) {
            // The pedal keeps the last note sounding (no retrigger).
            currentNote_ = pedalStack_[pedalCount_ - 1];
            setGate(true);
            return;
        }
        setGate(false);
        currentNote_ = -1;
        noteChanged_ = true;
        return;
    }
    if (newTop != currentNote_) {
        noteChanged_ = true;   // returned to a previously held note (legato)
    }
    currentNote_ = newTop;
    setGate(true);
}

void VoiceController::consumeEvents() {
    legatoEvent_ = false;
    triggerEvent_ = false;
    noteChanged_ = false;
}

} // namespace sh101
