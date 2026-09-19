// StepSequencer — the SH-101's on-board sequencer.
//
// Hardware basis (brief, "MIDI/voice behavior"): the CPU-based sequencer plays
// up to 100 steps (Roland specifications).  Like the arpeggiator it is a mono
// note source for the analog voice; key transpose is a note offset applied on
// top of the stored steps.
//
// Clocking follows the same policy as the arpeggiator: internal free-running
// clock by default, optional host-clock advance.
//
// [APPROX] Step gate length, tie semantics and the exact transpose interaction
// are modelled from the operator description rather than measured; the step
// count limit (100) is from the published specification.
#pragma once

#include "sh101/Constants.h"

namespace sh101 {

class StepSequencer {
public:
    static constexpr int kMaxSteps = 100;

    struct Step {
        int  note = 60;
        bool gate = true;   // false = rest
        bool tie = false;   // hold through the next step
    };

    struct Event {
        bool noteChange = false;
        int  note = -1;
        bool gateChange = false;
        bool gate = false;
    };

    void prepare(double sampleRate);
    void reset();

    void setEnabled(bool on) { enabled_ = on; }
    void setRate(double hz) { rate_ = clampd(hz, 0.05, 60.0); }
    void setLength(int steps) { length_ = clampi(steps, 1, kMaxSteps); }
    void setGateLength(double frac) { gateLength_ = clampd(frac, 0.05, 1.0); }
    void setTranspose(int semitones) { transpose_ = clampi(semitones, -48, 48); }
    void setExternalClock(bool ext) { externalClock_ = ext; }
    void advanceExternalClock() { externalTick_ = true; }

    void clear();
    void setStep(int index, int note, bool gate = true, bool tie = false);
    const Step& step(int index) const;

    int  length() const { return length_; }
    int  currentIndex() const { return index_; }
    int  currentNote() const { return currentNote_; }
    bool gateHigh() const { return gate_; }
    bool enabled() const { return enabled_; }
    int  transpose() const { return transpose_; }

    Event process();

private:
    void stepTo(int index);
    int  effectiveNote(int index) const;

    double sr_ = 48000.0;
    double rate_ = 5.0;
    double phase_ = 0.0;
    double gateLength_ = 0.5;
    bool   enabled_ = false;
    bool   externalClock_ = false;
    bool   externalTick_ = false;
    bool   started_ = false;
    int    length_ = 8;
    int    index_ = 0;
    int    transpose_ = 0;
    int    currentNote_ = -1;
    bool   gate_ = false;
    Step   steps_[kMaxSteps] = {};
};

} // namespace sh101
