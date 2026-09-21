// Arpeggiator and step sequencer validation.
//
// Validation list from the brief: "arp patterns and clocking", "sequencer
// playback semantics".
#include <cstdio>
#include <cstring>
#include <vector>

#include "TestFramework.h"
#include "TestHelpers.h"
#include "sh101/Arpeggiator.h"
#include "sh101/Params.h"
#include "sh101/SH101Engine.h"
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

// Peak of a rendered block, for the "has it stopped sounding" checks.
double blockPeak(const std::vector<float>& audio) {
    double p = 0.0;
    for (float v : audio) p = std::max(p, std::fabs(static_cast<double>(v)));
    return p;
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

// The v1.0 step-editor controls are host parameters: seqLength (1..100 steps),
// seqGate (gate fraction) and seqTranspose (-24..+24).  This checks the taper in
// both directions, including the endpoints the panel can reach.
SH101_TEST(seq_parameter_taper_roundtrip) {
    SH101Params p;
    applyNormalized(p, pSeqLength, 0.0);    CHECK(p.seqLength == 1);
    applyNormalized(p, pSeqLength, 1.0);    CHECK(p.seqLength == 100);
    applyNormalized(p, pSeqGate, 0.0);      CHECK_NEAR(p.seqGate, 0.05, 1.0e-9);
    applyNormalized(p, pSeqGate, 1.0);      CHECK_NEAR(p.seqGate, 1.0, 1.0e-9);
    applyNormalized(p, pSeqTranspose, 0.0); CHECK(p.seqTranspose == -24);
    applyNormalized(p, pSeqTranspose, 0.5); CHECK(p.seqTranspose == 0);
    applyNormalized(p, pSeqTranspose, 1.0); CHECK(p.seqTranspose == 24);

    // Engineering -> normalized -> engineering is exact at the integers.
    for (int steps : { 1, 8, 16, 37, 100 }) {
        SH101Params q;
        q.seqLength = steps;
        SH101Params back;
        applyNormalized(back, pSeqLength, getNormalized(q, pSeqLength));
        CHECK(back.seqLength == steps);
    }
    for (int semis : { -24, -7, 0, 12, 24 }) {
        SH101Params q;
        q.seqTranspose = semis;
        SH101Params back;
        applyNormalized(back, pSeqTranspose, getNormalized(q, pSeqTranspose));
        CHECK(back.seqTranspose == semis);
    }

    // The parameter table is append-only (session compatibility) and every entry
    // has a stable name: a missing name would be a null pointer a host would
    // crash on.
    CHECK(pSeqGate == pSeqLength + 1);
    CHECK(pSeqTranspose == pSeqLength + 2);
    for (int i = 0; i < kNumParams; ++i) {
        const char* name = paramName(i);
        CHECK(name != nullptr && std::strcmp(name, "?") != 0 && name[0] != '\0');
    }
    CHECK(std::strcmp(paramName(pSeqLength), "seqLength") == 0);
    CHECK(std::strcmp(paramName(pSeqGate), "seqGate") == 0);
    CHECK(std::strcmp(paramName(pSeqTranspose), "seqTranspose") == 0);
}

// The three controls must reach the sequencer the engine is actually running —
// the check that catches a missing line in applyParams.
SH101_TEST(seq_parameters_drive_the_engine) {
    SH101Engine engine;
    engine.prepare(48000.0, 1);
    engine.setDeterministicTestMode(true);

    SH101Params p;
    p.seqOn = true;
    p.seqRate = 10.0;        // 0.1 s per step: one 4800-sample block per step
    p.seqLength = 2;
    p.seqTranspose = 12;
    p.seqGate = 1.0;
    engine.setParams(p);
    engine.sequencer().setStep(0, 36, true, false);
    engine.sequencer().setStep(1, 38, true, false);
    engine.sequencer().setStep(2, 40, true, false);   // outside the length

    std::vector<float> buf(4800);

    engine.renderBlock(buf.data(), 4800);
    CHECK(engine.currentNote() == 48);          // 36 + 12 semitones
    engine.renderBlock(buf.data(), 4800);
    CHECK(engine.currentNote() == 50);          // 38 + 12
    engine.renderBlock(buf.data(), 4800);
    CHECK(engine.currentNote() == 48);          // length 2: step 2 is not played

    // Long enough for three steps, and the transpose applies there too.
    p.seqLength = 3;
    engine.setParams(p);
    engine.renderBlock(buf.data(), 4800);
    CHECK(engine.currentNote() == 50);
    engine.renderBlock(buf.data(), 4800);
    CHECK(engine.currentNote() == 52);          // 40 + 12

    // Gate length, measured through the engine's own gate state over whole
    // steps (step 0 is aligned by rendering one full step first).
    std::printf("      engine gate fractions: ");
    for (double requested : { 1.0, 0.5, 0.25, 0.05 }) {
        p.seqLength = 1;
        p.seqGate = requested;
        engine.setParams(p);
        engine.renderBlock(buf.data(), 4800);   // align to a step boundary
        int high = 0;
        for (int i = 0; i < 4800; ++i) {
            engine.renderSample();
            if (engine.gate()) ++high;
        }
        const double fraction = static_cast<double>(high) / 4800.0;
        std::printf("%.2f ", fraction);
        CHECK_NEAR(fraction, requested, 0.03);
    }
    std::printf("\n");

    // Transpose of 0 means the stored notes play untransposed.
    p.seqTranspose = 0;
    p.seqLength = 2;
    engine.setParams(p);
    engine.renderBlock(buf.data(), 4800);       // advances into step 1
    CHECK(engine.currentNote() == 38);
    engine.renderBlock(buf.data(), 4800);       // wraps to step 0, untransposed
    CHECK(engine.currentNote() == 36);
}

// ---------------------------------------------------------------------------
// Releasing the last key must stop the voice, whatever point of the arp step the
// release lands on.  The gate is high for the first part of every step, so a
// release inside that window is the case that matters: the pattern empties
// inside the arpeggiator, and nothing downstream is told the gate fell.
// ---------------------------------------------------------------------------
SH101_TEST(arp_stops_when_the_last_key_is_released) {
    const double sr = 48000.0;
    const int stepSamples = 4800;          // 10 Hz: one step is 0.1 s
    int stuck = 0;
    int cases = 0;
    int silentBeforeRelease = 0;
    double worstPeak = 0.0;
    int worstAt = -1;

    // The arpeggiator's first step lands one step after the key goes down, so
    // the release is placed inside the *second* step, swept across it.
    for (int offset = 0; offset < stepSamples; offset += 300) {
        SH101Engine engine;
        engine.prepare(sr, 1);
        engine.setDeterministicTestMode(true);
        SH101Params p;
        p.arpOn = true;
        p.arpRate = 10.0;
        p.arpMode = Arpeggiator::Up;
        p.arpOctaves = 1;
        engine.setParams(p);

        std::vector<float> buf(600);
        engine.noteOn(60, 1.0);

        int rendered = 0;
        const int releasePoint = stepSamples + offset;   // inside the second step
        double peakBeforeRelease = 0.0;
        while (rendered < releasePoint) {
            const int n = std::min<int>(600, releasePoint - rendered);
            engine.renderBlock(buf.data(), n);
            rendered += n;
            if (rendered > releasePoint - 1200) peakBeforeRelease = std::max(peakBeforeRelease, blockPeak(buf));
        }
        // The case is only meaningful if the arpeggiator was actually sounding.
        // (At the very start of a step the note has only just been gated on, so
        // the guard applies from one block in.)
        if (offset >= 600 && peakBeforeRelease < 0.05) ++silentBeforeRelease;

        engine.noteOff(60);

        // Three quarters of a second past the release: the envelope has had far
        // longer than its release time to fall.
        double tailPeak = 0.0;
        bool stuckGate = false;
        for (int block = 0; block < 60; ++block) {
            engine.renderBlock(buf.data(), 600);
            if (block >= 40) tailPeak = std::max(tailPeak, blockPeak(buf));
            if (block >= 40 && engine.gate()) stuckGate = true;
        }

        ++cases;
        if (tailPeak > 0.02 || stuckGate) {
            ++stuck;
            if (tailPeak > worstPeak) {
                worstPeak = tailPeak;
                worstAt = offset;
            }
        }
    }
    std::printf("      release swept over the step: %d of %d still sounding "
                "(worst peak %.4f, release at +%d samples), %d case(s) not sounding "
                "before the release\n",
                stuck, cases, worstPeak, worstAt, silentBeforeRelease);
    CHECK(silentBeforeRelease == 0);   // guards against a vacuous pass
    CHECK(stuck == 0);
}

// The same gate, reached from the other direction: switching the note source off
// while its gate is high must release the note too.
SH101_TEST(switching_a_note_source_off_releases_a_gated_note) {
    const double sr = 48000.0;

    // Arpeggiator: ARP ON -> OFF while its gate is high (600 samples into the
    // second step, i.e. inside the gate window).
    {
        SH101Engine engine;
        engine.prepare(sr, 1);
        engine.setDeterministicTestMode(true);
        SH101Params p;
        p.arpOn = true;
        p.arpRate = 10.0;
        engine.setParams(p);

        std::vector<float> buf(600);
        engine.noteOn(60, 1.0);
        double soundingPeak = 0.0;
        for (int block = 0; block < 9; ++block) {   // one step, then into the next
            engine.renderBlock(buf.data(), 600);
            if (block >= 8) soundingPeak = std::max(soundingPeak, blockPeak(buf));
        }
        CHECK(soundingPeak > 0.05);                 // it was sounding first
        engine.noteOff(60);
        p.arpOn = false;
        engine.setParams(p);

        double tailPeak = 0.0;
        for (int block = 0; block < 60; ++block) {
            engine.renderBlock(buf.data(), 600);
            if (block >= 40) tailPeak = std::max(tailPeak, blockPeak(buf));
        }
        std::printf("      arp switched off mid-gate: sounding %.4f, tail peak %.4f\n",
                    soundingPeak, tailPeak);
        CHECK(tailPeak < 0.02);
    }

    // Sequencer: SEQ ON -> OFF while its gate is high, the same way.
    {
        SH101Engine engine;
        engine.prepare(sr, 1);
        engine.setDeterministicTestMode(true);
        SH101Params p;
        p.seqOn = true;
        p.seqRate = 10.0;
        p.seqGate = 1.0;                            // gate high for the whole step
        engine.setParams(p);
        engine.sequencer().setStep(0, 60, true, false);
        engine.sequencer().setStep(1, 64, true, false);

        std::vector<float> buf(600);
        double soundingPeak = 0.0;
        for (int block = 0; block < 9; ++block) {   // one step, then into the next
            engine.renderBlock(buf.data(), 600);
            if (block >= 8) soundingPeak = std::max(soundingPeak, blockPeak(buf));
        }
        CHECK(soundingPeak > 0.05);
        p.seqOn = false;
        engine.setParams(p);

        double tailPeak = 0.0;
        for (int block = 0; block < 60; ++block) {
            engine.renderBlock(buf.data(), 600);
            if (block >= 40) tailPeak = std::max(tailPeak, blockPeak(buf));
        }
        std::printf("      sequencer switched off mid-gate: sounding %.4f, tail peak %.4f\n",
                    soundingPeak, tailPeak);
        CHECK(tailPeak < 0.02);
    }
}
