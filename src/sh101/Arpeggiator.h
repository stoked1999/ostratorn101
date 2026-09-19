// Arpeggiator — mono note source derived from held keys.
//
// Hardware basis (brief, "MIDI/voice behavior" / "arpeggiator behavior"): on the
// original, the keyboard CPU runs the arpeggiator and feeds the analog voice a
// gate + CV pair.  Here the arpeggiator is a note source feeding the same mono
// voice, so key priority/legato/portamento logic is shared.
//
// Modes: Up, Down, Up/Down, with an octave range.
//
// Clocking (modern-host policy): the internal free-running clock is the default
// and is always available; an external/host clock can be selected, in which case
// steps advance on explicit clock ticks and the rate control is ignored until
// the internal clock is re-selected.
//
// [APPROX] Gate length as a fraction of the step and the exact octave ordering
// are not documented in the sources used; both are parameters, not hard-coded.
#pragma once

#include "sh101/Constants.h"

namespace sh101 {

class Arpeggiator {
public:
    enum Mode { Up = 0, Down = 1, UpDown = 2 };
    static constexpr int kMaxHeld = 16;

    struct Event {
        bool noteChange = false;
        int  note = -1;
        bool gateChange = false;
        bool gate = false;
    };

    void prepare(double sampleRate);
    void reset();

    void setEnabled(bool on) { enabled_ = on; }
    void setMode(int mode) { mode_ = clampi(mode, 0, 2); rebuildPattern(); }
    void setRate(double hz) { rate_ = clampd(hz, 0.05, 60.0); }
    void setOctaves(int n) { octaves_ = clampi(n, 1, 3); rebuildPattern(); }
    void setGateLength(double frac) { gateLength_ = clampd(frac, 0.05, 0.95); }
    void setExternalClock(bool ext) { externalClock_ = ext; }
    void advanceExternalClock() { externalTick_ = true; }

    void setHeldNotes(const int* notes, int count);

    Event process();      // advance one sample, return events (if any)

    int  currentNote() const { return currentNote_; }
    bool gateHigh() const { return gate_; }
    bool enabled() const { return enabled_; }
    int  patternLength() const { return patternLength_; }

private:
    void rebuildPattern();
    void stepTo(int index);

    double sr_ = 48000.0;
    double rate_ = 5.0;
    double phase_ = 0.0;
    double gateLength_ = 0.5;
    bool   enabled_ = false;
    bool   externalClock_ = false;
    bool   externalTick_ = false;
    bool   started_ = false;
    int    mode_ = Up;
    int    octaves_ = 1;

    int    held_[kMaxHeld] = {};
    int    heldCount_ = 0;
    int    pattern_[kMaxHeld * 3] = {};
    int    patternLength_ = 0;
    int    index_ = 0;
    int    currentNote_ = -1;
    bool   gate_ = false;
};

} // namespace sh101
