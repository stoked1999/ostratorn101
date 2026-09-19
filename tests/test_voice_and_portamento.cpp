// Monophonic note engine and portamento validation.
//
// Validation list from the brief: "portamento timing and AUTO behavior",
// "note-priority/legato/gate/trigger behavior".
#include <cstdio>
#include <vector>

#include "TestFramework.h"
#include "TestHelpers.h"
#include "sh101/Portamento.h"
#include "sh101/SH101Engine.h"
#include "sh101/VoiceController.h"

using namespace sh101;
using namespace sh101test;

// Last-note priority with a held-note stack, and the return to the previous
// note when the newest key is released.
SH101_TEST(voice_last_note_priority_and_stack_return) {
    VoiceController v;
    v.prepare(48000.0);

    v.noteOn(60);
    CHECK(v.gateHigh());
    CHECK(v.currentNote() == 60);

    v.noteOn(64);
    CHECK(v.currentNote() == 64);
    CHECK(v.legato());              // second note arrived while gated

    v.noteOn(67);
    CHECK(v.currentNote() == 67);
    v.consumeEvents();

    v.noteOff(67);                  // back to the previously held note
    CHECK(v.gateHigh());
    CHECK(v.currentNote() == 64);
    CHECK(!v.triggerEvent());       // returning to a held note is not a new note

    v.noteOff(64);
    CHECK(v.currentNote() == 60);
    v.noteOff(60);
    CHECK(!v.gateHigh());
    CHECK(v.currentNote() == -1);
}

// A note-on always raises a trigger; the envelope trigger mode decides what to
// do with it.  Gate state and legato reporting are the other two signals.
SH101_TEST(voice_gate_trigger_and_legato_flags) {
    VoiceController v;
    v.prepare(48000.0);

    v.noteOn(60);
    CHECK(v.triggerEvent());
    CHECK(!v.legato());
    CHECK(v.gateHigh());
    v.consumeEvents();

    v.noteOn(62);
    CHECK(v.triggerEvent());
    CHECK(v.legato());
    v.consumeEvents();

    v.noteOff(62);
    v.noteOff(60);
    CHECK(!v.gateHigh());
    // The stack must be empty: pressing the same note again is not legato.
    v.noteOn(60);
    CHECK(!v.legato());
}

// Sustain pedal as a host convenience: it must hold notes without breaking the
// mono gate/legato logic.
SH101_TEST(voice_sustain_pedal_holds_notes) {
    VoiceController v;
    v.prepare(48000.0);

    v.setSustainPedal(true);
    v.noteOn(60);
    v.noteOff(60);
    CHECK(v.gateHigh());            // pedal keeps it sounding
    CHECK(v.currentNote() == 60);

    v.setSustainPedal(false);
    CHECK(!v.gateHigh());           // pedal release ends the note
    CHECK(v.currentNote() == -1);

    // With the pedal up, normal playing is unaffected.
    v.noteOn(64);
    v.noteOff(64);
    CHECK(!v.gateHigh());
}

SH101_TEST(voice_all_notes_off_is_a_panic) {
    VoiceController v;
    v.prepare(48000.0);
    for (int n : { 60, 64, 67 }) v.noteOn(n);
    v.allNotesOff();
    CHECK(!v.gateHigh());
    CHECK(v.currentNote() == -1);
    CHECK(v.heldCount() == 0);
}

// Portamento timing (ON) and the AUTO/DEFERRED behaviour: AUTO glides only in a
// legato context.
SH101_TEST(portamento_timing_and_modes) {
    const double sr = 48000.0;

    // OFF: the CV is at the target immediately.
    Portamento pm;
    pm.prepare(sr);
    pm.setMode(Portamento::Off);
    pm.setTime(1.0);
    pm.reset(0.0);
    CHECK_NEAR(pm.process(1.0), 1.0, 1e-12);

    // ON: 99% of the interval is covered in the set time.
    for (double t : { 0.2, 1.0, 3.0 }) {
        Portamento p2;
        p2.prepare(sr);
        p2.setMode(Portamento::On);
        p2.setTime(t);
        p2.reset(0.0);
        int samples = 0;
        const int limit = static_cast<int>(sr * (t * 2.0 + 0.2));
        while (samples < limit && std::fabs(1.0 - p2.value()) > 0.01) {
            p2.process(1.0);
            ++samples;
        }
        const double measured = static_cast<double>(samples) / sr;
        std::printf("      portamento %.2f s -> measured %.3f s to 99%%\n", t, measured);
        CHECK_NEAR(measured, t, std::max(0.01, t * 0.15));
    }

    // Time 0 must be an immediate jump even in ON mode.
    Portamento p3;
    p3.prepare(sr);
    p3.setMode(Portamento::On);
    p3.setTime(0.0);
    p3.reset(0.0);
    CHECK_NEAR(p3.process(1.0), 1.0, 1e-12);

    // AUTO without a legato context: immediate.
    Portamento p4;
    p4.prepare(sr);
    p4.setMode(Portamento::Auto);
    p4.setTime(1.0);
    p4.setAutoEligible(false);
    p4.reset(0.0);
    CHECK_NEAR(p4.process(1.0), 1.0, 1e-12);

    // AUTO with a legato context: glides.
    p4.setAutoEligible(true);
    p4.reset(0.0);
    const double firstStep = p4.process(1.0);
    CHECK_LT(firstStep, 0.5);
    CHECK(p4.gliding());
}

namespace {
// Renders the engine and reports the pitch CV trace for the portamento tests.
std::vector<double> renderPitchTrace(SH101Engine& engine, int samples) {
    std::vector<double> cv;
    cv.reserve(samples);
    for (int i = 0; i < samples; ++i) {
        engine.renderSample();
        cv.push_back(engine.pitchCV());
    }
    return cv;
}

SH101Params portamentoTestParams(int mode, double timeSeconds) {
    SH101Params p;
    p.sawLevel = 1.0;
    p.pulseLevel = 0.0;
    p.cutoff = 4000.0;
    p.resonance = 0.0;
    p.attack = 0.002;
    p.decay = 1.0;
    p.sustain = 1.0;
    p.release = 0.05;
    p.portamentoMode = mode;
    p.portamentoTime = timeSeconds;
    p.vcoModDep = 0.0;
    p.keyTrack = 0.0;
    p.filterEnvAmt = 0.0;
    return p;
}
} // namespace

// Engine-level: gliding between two notes takes the set time, and AUTO glides
// only when the new note is taken legato (key held).
SH101_TEST(engine_portamento_on_and_auto_behaviour) {
    const double sr = 48000.0;

    // ON, 0.5 s.
    {
        SH101Engine engine;
        engine.prepare(sr, 1);
        SH101Params p = portamentoTestParams(Portamento::On, 0.5);
        engine.setParams(p);
        engine.noteOn(48, 1.0);
        renderPitchTrace(engine, static_cast<int>(sr * 1.0));
        const double targetCv = midiNoteToCV(60.0);
        engine.noteOn(60, 1.0);
        int samples = 0;
        const int limit = static_cast<int>(sr * 2.0);
        // Absolute tolerance: the target CV for MIDI 60 is 0.000 V, so a
        // relative tolerance would never be satisfied.
        while (samples < limit && std::fabs(engine.pitchCV() - targetCv) > 0.01) {
            engine.renderSample();
            ++samples;
        }
        const double measured = static_cast<double>(samples) / sr;
        std::printf("      engine portamento 0.50 s -> %.3f s to 1%% of 1 V\n", measured);
        CHECK_NEAR(measured, 0.5, 0.12);
    }

    // AUTO, non-legato: the pitch jumps.
    {
        SH101Engine engine;
        engine.prepare(sr, 1);
        SH101Params p = portamentoTestParams(Portamento::Auto, 0.5);
        engine.setParams(p);
        engine.noteOn(48, 1.0);
        renderPitchTrace(engine, static_cast<int>(sr * 0.5));
        engine.noteOff(48);
        renderPitchTrace(engine, static_cast<int>(sr * 0.2));
        engine.noteOn(60, 1.0);
        engine.renderSample();
        CHECK_NEAR(engine.pitchCV(), midiNoteToCV(60.0), 1e-6);
    }

    // AUTO, legato: the pitch glides.
    {
        SH101Engine engine;
        engine.prepare(sr, 1);
        SH101Params p = portamentoTestParams(Portamento::Auto, 0.5);
        engine.setParams(p);
        engine.noteOn(48, 1.0);
        renderPitchTrace(engine, static_cast<int>(sr * 0.5));
        engine.noteOn(60, 1.0);          // legato: key 48 still held
        engine.renderSample();
        const double after = engine.pitchCV();
        CHECK_LT(after, midiNoteToCV(60.0) - 0.1);
        CHECK_GT(after, midiNoteToCV(48.0) - 0.1);
    }
}

// Trigger mode behaviour at engine level: GATE+TRIG restarts the envelope when a
// new note is taken legato; GATE lets it continue running.
SH101_TEST(engine_trigger_modes_control_envelope_restart) {
    const double sr = 48000.0;

    // Short attack, medium decay, no sustain: after 150 ms the envelope has
    // decayed to a low value, so a restart is unmistakable.
    auto run = [&](int triggerMode) {
        SH101Engine engine;
        engine.prepare(sr, 1);
        SH101Params p;
        p.sawLevel = 1.0;
        p.pulseLevel = 0.0;
        p.cutoff = 3000.0;
        p.attack = 0.005;
        p.decay = 0.3;
        p.sustain = 0.0;
        p.release = 0.1;
        p.envTrigger = triggerMode;
        engine.setParams(p);
        engine.noteOn(60, 1.0);
        for (int i = 0; i < static_cast<int>(sr * 0.15); ++i) engine.renderSample();
        const double before = engine.envelopeValue();
        engine.noteOn(64, 1.0);            // legato note change
        for (int i = 0; i < static_cast<int>(sr * 0.01); ++i) engine.renderSample();
        return std::make_pair(before, engine.envelopeValue());
    };

    const auto gateTrig = run(SH101Envelope::GateAndTrig);
    const auto gateOnly = run(SH101Envelope::GateOnly);
    std::printf("      legato note change: GATE+TRIG %.3f -> %.3f | GATE %.3f -> %.3f\n",
                gateTrig.first, gateTrig.second, gateOnly.first, gateOnly.second);

    CHECK_LT(gateTrig.first, 0.2);
    CHECK_GT(gateTrig.second, 0.6);       // attack restarted from the current value
    CHECK_LT(gateOnly.second, 0.2);       // envelope just kept decaying

    // A fresh note after silence must start an attack in both modes.
    for (int mode : { SH101Envelope::GateAndTrig, SH101Envelope::GateOnly }) {
        SH101Engine engine;
        engine.prepare(sr, 1);
        SH101Params p;
        p.sawLevel = 1.0;
        p.cutoff = 3000.0;
        p.attack = 0.2;
        p.sustain = 1.0;
        p.envTrigger = mode;
        engine.setParams(p);
        engine.noteOn(60, 1.0);
        for (int i = 0; i < static_cast<int>(sr * 0.3); ++i) engine.renderSample();
        CHECK_GT(engine.envelopeValue(), 0.5);
    }
}
