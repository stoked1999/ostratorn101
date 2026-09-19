// Arpeggiator and step sequencer validation.
//
// Validation list from the brief: "arp patterns and clocking", "sequencer
// playback semantics".
#include <cstdio>
#include <vector>

#include "TestFramework.h"
#include "TestHelpers.h"
#include "sh101/Arpeggiator.h"
#include "sh101/StepSequencer.h"

using namespace sh101;
using namespace sh101test;

namespace {

// Collects the note-on sequence produced by a note source over `seconds`.
template <class Source>
std::vector<int> collectNotes(Source& src, double sr, double seconds) {
    std::vector<int> notes;
    const int n = static_cast<int>(sr * seconds);
    for (int i = 0; i < n; ++i) {
        const auto ev = src.process();
        if (ev.noteChange && ev.note >= 0) notes.push_back(ev.note);
    }
    return notes;
}

std::vector<int> take(const std::vector<int>& v, size_t count) {
    std::vector<int> out(v.begin(), v.begin() + std::min(count, v.size()));
    return out;
}

} // namespace

// Up / Down / Up-Down orders from a held chord.
SH101_TEST(arp_patterns_up_down_updown) {
    const double sr = 48000.0;
    const int held[3] = { 60, 64, 67 };

    for (int mode : { Arpeggiator::Up, Arpeggiator::Down, Arpeggiator::UpDown }) {
        Arpeggiator arp;
        arp.prepare(sr);
        arp.setRate(10.0);          // 0.1 s per step
        arp.setOctaves(1);
        arp.setHeldNotes(held, 3);
        arp.setMode(mode);
        arp.setEnabled(true);

        const std::vector<int> notes = collectNotes(arp, sr, 0.35);
        const std::vector<int> actual = take(notes, 3);
        std::vector<int> expected;
        if (mode == Arpeggiator::Up) expected = { 60, 64, 67 };
        else if (mode == Arpeggiator::Down) expected = { 67, 64, 60 };
        else expected = { 60, 64, 67 };   // up-down starts upward

        std::printf("      mode %d: ", mode);
        for (int v : actual) std::printf("%d ", v);
        std::printf("\n");
        CHECK(actual == expected);

        if (mode == Arpeggiator::UpDown) {
            // ...and then descends: 64, 60, 64, ...
            const std::vector<int> more = collectNotes(arp, sr, 0.25);
            CHECK(!more.empty());
            CHECK(more[0] == 64);
        }
    }
}

// Octave range extends the pattern upward in octaves.
SH101_TEST(arp_octave_range) {
    const double sr = 48000.0;
    const int held[2] = { 48, 52 };
    Arpeggiator arp;
    arp.prepare(sr);
    arp.setRate(10.0);
    arp.setOctaves(2);
    arp.setHeldNotes(held, 2);
    arp.setMode(Arpeggiator::Up);
    arp.setEnabled(true);

    const std::vector<int> notes = take(collectNotes(arp, sr, 0.45), 4);
    const std::vector<int> expected = { 48, 52, 60, 64 };
    CHECK(notes == expected);
    CHECK(arp.patternLength() == 4);
}

// Gate length: the gate is high for the configured fraction of each step.  The
// measurement starts at the first step boundary and spans a whole number of
// steps, so window edges cannot bias the duty cycle.
SH101_TEST(arp_gate_length) {
    const double sr = 48000.0;
    const int held[1] = { 60 };
    Arpeggiator arp;
    arp.prepare(sr);
    arp.setRate(10.0);
    arp.setGateLength(0.5);
    arp.setHeldNotes(held, 1);
    arp.setEnabled(true);

    // Advance to the first step boundary.
    while (!arp.process().gateChange) { /* spin to the first gate rise */ }

    const int stepSamples = static_cast<int>(sr * 0.1);
    int high = 0;
    const int steps = 8;
    for (int i = 0; i < stepSamples * steps; ++i) {
        arp.process();
        if (arp.gateHigh()) ++high;
    }
    const double fraction = static_cast<double>(high) / (stepSamples * steps);
    std::printf("      gate high fraction: %.3f (requested 0.500)\n", fraction);
    CHECK_NEAR(fraction, 0.5, 0.02);
}

// Host-clock sync is optional; with the external clock selected, steps advance
// only on clock ticks.  Free-running timing stays the default.
SH101_TEST(arp_host_clock_sync_is_optional) {
    const double sr = 48000.0;
    const int held[3] = { 60, 64, 67 };
    Arpeggiator arp;
    arp.prepare(sr);
    arp.setRate(10.0);
    arp.setHeldNotes(held, 3);
    arp.setMode(Arpeggiator::Up);
    arp.setExternalClock(true);
    arp.setEnabled(true);

    // Without ticks, nothing happens.
    int eventsWithoutClock = 0;
    for (int i = 0; i < 9600; ++i) {
        const auto ev = arp.process();
        if (ev.noteChange || ev.gateChange) ++eventsWithoutClock;
    }
    CHECK(eventsWithoutClock == 0);

    // Each tick advances exactly one step.
    arp.advanceExternalClock();
    CHECK(arp.process().note == 60);
    arp.advanceExternalClock();
    CHECK(arp.process().note == 64);
    arp.advanceExternalClock();
    CHECK(arp.process().note == 67);

    // Back to the internal clock: it runs again.
    arp.setExternalClock(false);
    const std::vector<int> notes = take(collectNotes(arp, sr, 0.25), 2);
    CHECK(notes.size() == 2);
}

// Sequencer playback: note order, rests, ties and transpose.
SH101_TEST(seq_playback_semantics) {
    const double sr = 48000.0;
    StepSequencer seq;
    seq.prepare(sr);
    seq.setRate(10.0);          // 0.1 s per step
    seq.setLength(4);
    seq.setStep(0, 36, true, false);
    seq.setStep(1, 43, false, false);   // rest
    seq.setStep(2, 48, true, false);
    seq.setStep(3, 50, true, true);     // tied into the next cycle
    seq.setEnabled(true);

    // Notes: 36, then 43 (the rest produces no gate but the sequencer keeps
    // running), 48, 50.
    const std::vector<int> notes = take(collectNotes(seq, sr, 0.45), 4);
    const std::vector<int> expected = { 36, 43, 48, 50 };
    CHECK(notes == expected);
    CHECK(seq.length() == 4);

    // Gate pattern per step index: step 0 gated, step 1 rest (low), step 2
    // gated, step 3 tied so it stays high into the next step 0.  Gates are
    // accumulated against the sequencer's own step index, so window alignment
    // cannot bias the result.
    std::vector<double> high(4, 0.0), total(4, 0.0);
    const int n = static_cast<int>(sr * 3.0);
    for (int i = 0; i < n; ++i) {
        seq.process();
        const int idx = seq.currentIndex();
        if (idx >= 0 && idx < 4) {
            total[idx] += 1.0;
            if (seq.gateHigh()) high[idx] += 1.0;
        }
    }
    std::printf("      gate fraction per step: %.2f %.2f %.2f %.2f\n", high[0] / total[0],
                high[1] / total[1], high[2] / total[2], high[3] / total[3]);
    CHECK_NEAR(high[0] / total[0], 0.5, 0.06);   // gate length default 0.5
    CHECK_NEAR(high[1] / total[1], 0.0, 0.02);   // rest
    CHECK_NEAR(high[2] / total[2], 0.5, 0.06);
    CHECK_NEAR(high[3] / total[3], 1.0, 0.02);   // tied: no gate drop

    // Transpose shifts playback by semitones.
    StepSequencer transposedSeq;
    transposedSeq.prepare(sr);
    transposedSeq.setRate(10.0);
    transposedSeq.setLength(4);
    transposedSeq.setStep(0, 36, true, false);
    transposedSeq.setStep(1, 43, false, false);
    transposedSeq.setStep(2, 48, true, false);
    transposedSeq.setStep(3, 50, true, true);
    transposedSeq.setTranspose(12);
    transposedSeq.setEnabled(true);
    const std::vector<int> transposed = take(collectNotes(transposedSeq, sr, 0.25), 2);
    CHECK(!transposed.empty());
    CHECK(transposed[0] == 48);
}

// Capacity and clocking: Roland's specification is up to 100 steps, and the
// external clock option follows the same policy as the arpeggiator.
SH101_TEST(seq_capacity_and_clocking) {
    StepSequencer seq;
    seq.prepare(48000.0);
    CHECK(seq.kMaxSteps == 100);

    seq.setLength(100);
    CHECK(seq.length() == 100);
    seq.setStep(99, 72, true, false);
    CHECK(seq.step(99).note == 72);
    seq.setStep(1000, 60);            // out of range: ignored, no crash
    CHECK(seq.step(99).note == 72);

    // External clock: one step per tick, no free-running advance.
    seq.setExternalClock(true);
    seq.setEnabled(true);
    int eventsWithoutClock = 0;
    for (int i = 0; i < 4800; ++i) {
        const auto ev = seq.process();
        if (ev.noteChange || ev.gateChange) ++eventsWithoutClock;
    }
    CHECK(eventsWithoutClock == 0);
    seq.advanceExternalClock();
    const auto ev = seq.process();
    CHECK(ev.noteChange);
    CHECK(ev.note == seq.step(0).note);
}
