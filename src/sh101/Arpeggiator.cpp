#include "sh101/Arpeggiator.h"

namespace sh101 {

void Arpeggiator::prepare(double sampleRate) {
    sr_ = sampleRate > 0.0 ? sampleRate : 48000.0;
    reset();
}

void Arpeggiator::reset() {
    phase_ = 0.0;
    index_ = 0;
    currentNote_ = -1;
    gate_ = false;
    externalTick_ = false;
    started_ = false;
    heldCount_ = 0;
    rebuildPattern();
}

void Arpeggiator::setHeldNotes(const int* notes, int count) {
    heldCount_ = clampi(count, 0, kMaxHeld);
    for (int i = 0; i < heldCount_; ++i) held_[i] = notes[i];
    // Sort ascending (insertion sort; at most 16 entries).
    for (int i = 1; i < heldCount_; ++i) {
        const int key = held_[i];
        int j = i - 1;
        while (j >= 0 && held_[j] > key) {
            held_[j + 1] = held_[j];
            --j;
        }
        held_[j + 1] = key;
    }
    rebuildPattern();
}

void Arpeggiator::rebuildPattern() {
    patternLength_ = 0;
    if (heldCount_ <= 0) {
        currentNote_ = -1;
        gate_ = false;
        return;
    }
    // Base sequence by mode.
    // Up/Down concatenate a mirrored copy, so the scratch sequence needs room
    // for 2 * kMaxHeld entries (UpDown with 16 held notes produces 30).
    int base[2 * kMaxHeld] = {};
    int baseLen = 0;
    for (int i = 0; i < heldCount_; ++i) base[baseLen++] = held_[i];
    if (mode_ == Down) {
        for (int i = 0, j = baseLen - 1; i < j; ++i, --j) {
            const int t = base[i];
            base[i] = base[j];
            base[j] = t;
        }
    } else if (mode_ == UpDown && baseLen > 1) {
        for (int i = baseLen - 2; i >= 1; --i) base[baseLen++] = base[i];
    }

    for (int oct = 0; oct < octaves_; ++oct) {
        for (int i = 0; i < baseLen; ++i) {
            if (patternLength_ >= kMaxHeld * 3) break;
            pattern_[patternLength_++] = base[i] + 12 * oct;
        }
    }
    if (index_ >= patternLength_) index_ = 0;
}

void Arpeggiator::stepTo(int index) {
    if (patternLength_ <= 0) {
        currentNote_ = -1;
        return;
    }
    index_ = ((index % patternLength_) + patternLength_) % patternLength_;
    currentNote_ = pattern_[index_];
}

Arpeggiator::Event Arpeggiator::process() {
    Event ev;
    if (!enabled_ || patternLength_ <= 0) {
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

    if (stepBoundary) {
        // The first clock after (re)start plays pattern step 0; later clocks
        // advance.  Without this the first note of the pattern would be skipped.
        if (!started_) {
            started_ = true;
            stepTo(0);
        } else {
            stepTo(index_ + 1);
        }
        ev.noteChange = true;
        ev.note = currentNote_;
        if (!gate_) {
            gate_ = true;
            ev.gateChange = true;
            ev.gate = true;
        }
    }

    // Gate falls part-way through the step (gate length fraction of the step).
    if (!externalClock_ && gate_ && phase_ >= gateLength_) {
        gate_ = false;
        ev.gateChange = true;
        ev.gate = false;
    }

    return ev;
}

} // namespace sh101
