// VoiceController — monophonic note stack: priority, legato, gate, trigger.
//
// Hardware basis (brief, "MIDI/voice behavior"): the original uses a
// TMP80C49-class CPU for keyboard assignment and the audio path is analog.  The
// brief says not to emulate the CPU instruction-by-instruction, but to reproduce
// observable behaviour: monophonic note priority/legato, gate/trigger behaviour,
// portamento modes, arpeggiator and sequencer behaviour, key transpose.
//
// Implemented here:
//   * last-note priority with a held-note stack (a fixed array — no audio-thread
//     allocation),
//   * gate high while at least one key is held,
//   * an ordered queue of gate transitions.  This matters because hosts deliver
//     a block of MIDI events *before* rendering that block: if two note events
//     fall inside one rendered sample (a note shorter than the block, which is
//     routine at small buffer sizes) a state-only interface would never show the
//     edge, and the envelope would be left running with the gate already low,
//   * `legato` flag: a new note arrived while the gate was already high,
//   * `trigger` one-shot flag: an event that the envelope trigger mode decides
//     what to do with (GATE+TRIG retriggers on every note, GATE only on the
//     gate's rising edge),
//   * sustain pedal as a host convenience that does not break the mono
//     gate/legato logic (modern-host policy),
//   * velocity is captured but does not influence the sound unless explicitly
//     enabled (modern-host policy: velocity/aftertouch OFF by default).
#pragma once

#include "sh101/Constants.h"

namespace sh101 {

class VoiceController {
public:
    static constexpr int kMaxNotes = 16;
    static constexpr int kMaxGateEvents = 32;

    void prepare(double sampleRate) { (void)sampleRate; reset(); }
    void reset();

    void noteOn(int midiNote, double velocity = 1.0);
    void noteOff(int midiNote);
    void allNotesOff();                  // panic / All Notes Off
    void setSustainPedal(bool down);

    bool gateHigh() const { return gate_; }
    int  currentNote() const { return gate_ ? currentNote_ : -1; }
    bool legato() const { return legatoEvent_; }
    bool triggerEvent() const { return triggerEvent_; }
    bool noteChanged() const { return noteChanged_; }
    double velocity() const { return velocity_; }

    int  heldCount() const { return count_; }
    const int* heldNotes() const { return stack_; }
    bool sustainDown() const { return pedalDown_; }

    // Pops the oldest pending gate transition.  Returns false when the queue is
    // empty.  `isRise` is true for closed -> open.
    bool takeGateEvent(bool& isRise);
    int  pendingGateEvents() const { return gateEventCount_; }

    // Called by the engine once per sample after it has consumed the events.
    void consumeEvents();

private:
    void setGate(bool on);
    void setGateFromState();
    void pushGateEvent(bool isRise);
    int  topOfStack() const { return count_ > 0 ? stack_[count_ - 1] : -1; }

    int  stack_[kMaxNotes] = {};
    int  count_ = 0;
    int  pedalStack_[kMaxNotes] = {};
    int  pedalCount_ = 0;
    bool pedalDown_ = false;

    bool gateEventRise_[kMaxGateEvents] = {};
    int  gateEventHead_ = 0;
    int  gateEventCount_ = 0;

    bool gate_ = false;
    int  currentNote_ = -1;
    bool legatoEvent_ = false;
    bool triggerEvent_ = false;
    bool noteChanged_ = false;
    double velocity_ = 1.0;
};

} // namespace sh101

