// Host-adapter validation: MIDI translation, automation and buffer handling.
//
// This covers the platform-independent half of the brief's step 9 (the VST3
// wrapper) plus the "Modern-host policy" requirements: note on/off, bend range,
// CC automation, sustain pedal, All Notes Off, optional host clock, and no stereo
// widening.
#include <cstdio>
#include <vector>

#include "TestFramework.h"
#include "TestHelpers.h"
#include "plugin/SH101HostAdapter.h"

using namespace sh101;
using namespace sh101test;

namespace {

SH101Params adapterPatch() {
    SH101Params p;
    p.sawLevel = 1.0;
    p.pulseLevel = 0.0;
    p.subLevel = 0.0;
    p.noiseLevel = 0.0;
    p.cutoff = 2000.0;
    p.resonance = 0.0;
    p.attack = 0.005;
    p.decay = 1.0;
    p.sustain = 1.0;
    p.release = 0.05;
    p.volume = 0.8;
    return p;
}

double rmsOf(const std::vector<float>& x) {
    double s = 0.0;
    for (float v : x) s += static_cast<double>(v) * v;
    return std::sqrt(s / static_cast<double>(x.size()));
}

// Renders `settle` samples into scratch, then returns the RMS of the following
// `measure` samples.  Measuring a window *after* the release has finished is the
// only way to assert "silent now"; a window that still contains the release tail
// is not silent by definition.
double steadyRms(SH101HostAdapter& a, std::vector<float>& scratch, int settle, int measure) {
    const int n = std::max(settle, measure);
    scratch.assign(static_cast<size_t>(n), 0.0f);
    a.renderBlock(scratch.data(), n);
    std::vector<float> window(scratch.begin() + n - measure, scratch.end());
    return rmsOf(window);
}

} // namespace

// Note On/Off through raw MIDI drives the mono voice; velocity 0 is a Note Off.
SH101_TEST(adapter_midi_note_on_off) {
    SH101HostAdapter a;
    a.prepare(48000.0, 512, 1);
    a.paramsRef() = adapterPatch();
    a.commitParameters();

    std::vector<float> buf(9600);
    a.renderBlock(buf.data(), 9600);
    CHECK_LT(rmsOf(buf), 1e-3);                       // silent before any note

    a.handleMidiMessage(0x90, 60, 100);               // Note On
    a.renderBlock(buf.data(), 9600);
    CHECK_GT(rmsOf(buf), 0.01);
    CHECK(a.lastMidiNote() == 60);

    a.handleMidiMessage(0x80, 60, 0);                 // Note Off
    // Let the release finish, then verify silence.
    CHECK_LT(steadyRms(a, buf, 9600, 4800), 1e-3);

    // Note On with velocity 0 must behave as Note Off (nothing sounds).
    a.handleMidiMessage(0x90, 62, 0);
    CHECK_LT(steadyRms(a, buf, 9600, 4800), 1e-3);

    // Multiple held notes stay monophonic: last-note priority.
    a.handleMidiMessage(0x90, 60, 100);
    a.handleMidiMessage(0x90, 67, 100);
    a.renderBlock(buf.data(), 4800);
    CHECK(a.engine().currentNote() == 67);
    a.handleMidiMessage(0x80, 67, 0);
    a.renderBlock(buf.data(), 2400);
    CHECK(a.engine().currentNote() == 60);
    a.handleMidiMessage(0xB0, 123, 0);                // All Notes Off
    CHECK_LT(steadyRms(a, buf, 9600, 4800), 1e-3);
    CHECK(!a.engine().gate());
}

// Sustain pedal and panic handling.
SH101_TEST(adapter_sustain_pedal_and_panic) {
    SH101HostAdapter a;
    a.prepare(48000.0, 512, 1);
    a.paramsRef() = adapterPatch();
    a.commitParameters();

    std::vector<float> buf(9600);
    a.handleMidiMessage(0xB0, 64, 127);               // pedal down
    a.handleMidiMessage(0x90, 60, 100);
    a.handleMidiMessage(0x80, 60, 0);
    a.renderBlock(buf.data(), 9600);
    CHECK_GT(rmsOf(buf), 0.01);                       // pedal holds the note

    a.handleMidiMessage(0xB0, 64, 0);                 // pedal up
    CHECK_LT(steadyRms(a, buf, 9600, 4800), 1e-3);

    a.handleMidiMessage(0x90, 60, 100);
    a.handleMidiMessage(0xB0, 120, 0);                // All Sound Off
    CHECK_LT(steadyRms(a, buf, 9600, 4800), 1e-3);
}

// Pitch bend: full-scale bend equals the configured range (default +/-2
// semitones, matching the bender).
SH101_TEST(adapter_pitch_bend_range) {
    SH101HostAdapter a;
    a.prepare(48000.0, 512, 1);
    SH101Params p = adapterPatch();
    p.cutoff = 8000.0;
    a.paramsRef() = p;
    a.commitParameters();
    CHECK(a.pitchBendRangeSemitones() == 2);

    std::vector<float> buf(4800);
    a.handleMidiMessage(0x90, 60, 100);
    a.renderBlock(buf.data(), 4800);
    const double base = a.engine().oscillatorHz();
    CHECK_NEAR(base, cvToFrequency(midiNoteToCV(60)), 1.0);

    a.handleMidiMessage(0xE0, 127, 127);              // bend up
    a.renderBlock(buf.data(), 4800);
    CHECK_NEAR(a.pitchBendSemitones(), 2.0, 0.01);
    CHECK_NEAR(a.engine().oscillatorHz(), base * std::pow(2.0, 2.0 / 12.0), base * 0.002);

    a.handleMidiMessage(0xE0, 0, 0);                  // bend down
    a.renderBlock(buf.data(), 4800);
    CHECK_NEAR(a.pitchBendSemitones(), -2.0, 0.02);

    a.handleMidiMessage(0xE0, 0, 64);                 // centre
    a.renderBlock(buf.data(), 4800);
    CHECK_NEAR(a.pitchBendSemitones(), 0.0, 0.01);

    // A different bend range is honoured (host-side configuration).
    a.setPitchBendRangeSemitones(12);
    a.handleMidiMessage(0xE0, 127, 127);
    a.renderBlock(buf.data(), 4800);
    CHECK_NEAR(a.pitchBendSemitones(), 12.0, 0.05);
}

// Automation: normalized parameters map onto the documented ranges.
SH101_TEST(adapter_parameter_automation) {
    SH101HostAdapter a;
    a.prepare(48000.0, 512, 1);

    a.setParameter(pCutoff, 1.0);
    CHECK_NEAR(a.params().cutoff, 20000.0, 1.0);
    a.setParameter(pCutoff, 0.0);
    CHECK_NEAR(a.params().cutoff, 10.0, 1e-6);
    a.setParameter(pAttack, 1.0);
    CHECK_NEAR(a.params().attack, 4.0, 1e-6);
    a.setParameter(pVolume, 0.0);
    CHECK_NEAR(a.params().volume, 0.0, 1e-9);
    CHECK_NEAR(a.getParameter(pVolume), 0.0, 1e-9);

    // Round-trip a few parameters through normalized get/set.
    for (int id : { pCutoff, pResonance, pAttack, pRelease, pLfoRate, pPortamentoTime }) {
        a.setParameter(id, 0.37);
        CHECK_NEAR(a.getParameter(id), 0.37, 0.02);
    }

    // Out-of-range parameter ids must be ignored, not written out of bounds.
    a.setParameter(-1, 1.0);
    a.setParameter(kNumParams + 5, 1.0);
    CHECK_NEAR(a.getParameter(-1), 0.0, 1e-12);

    // Bulk automation (what a plugin's processBlock calls): one commit for the
    // whole parameter set.
    float values[kNumParams];
    for (int i = 0; i < kNumParams; ++i) values[i] = static_cast<float>(0.5);
    a.setParametersNormalized(values, kNumParams);
    CHECK_NEAR(a.params().cutoff, taperExponential(0.5, 10.0, 20000.0), 1.0);
    values[pCutoff] = 1.0f;
    a.setParametersNormalized(values, kNumParams);
    CHECK_NEAR(a.params().cutoff, 20000.0, 1.0);
    a.setParametersNormalized(nullptr, kNumParams);   // must be a no-op, not a crash
    CHECK_NEAR(a.params().cutoff, 20000.0, 1.0);

    // CC 1 (mod wheel) is a modulation-depth control, and CC 5 sets portamento.
    a.handleMidiMessage(0xB0, 1, 127);
    CHECK_NEAR(a.params().vcoModDep, 1.0, 1e-9);
    a.handleMidiMessage(0xB0, 5, 64);
    CHECK_NEAR(a.params().portamentoTime, 5.0 * 64.0 / 127.0, 1e-9);
}

// Rendering into a multi-channel buffer must not add width: the SH-101 is
// monophonic and the brief forbids stereo widening in the core emulation.
SH101_TEST(adapter_multichannel_render_is_mono) {
    SH101HostAdapter a;
    a.prepare(48000.0, 512, 2);
    a.paramsRef() = adapterPatch();
    a.commitParameters();
    a.handleMidiMessage(0x90, 60, 100);

    const int n = 2048;
    std::vector<float> left(n), right(n);
    float* channels[2] = { left.data(), right.data() };
    a.renderBlock(channels, 2, n);

    bool identical = true;
    for (int i = 0; i < n; ++i) {
        if (left[i] != right[i]) identical = false;
    }
    CHECK(identical);
    CHECK_GT(rmsOf(left), 0.001);

    // A single null channel pointer must not be dereferenced.
    float* bad[2] = { nullptr, nullptr };
    a.renderBlock(bad, 2, n);
    CHECK_GT(rmsOf(left), 0.001);
}

// MIDI buffer parsing, including 2-byte and truncated messages.
SH101_TEST(adapter_midi_buffer_parsing) {
    SH101HostAdapter a;
    a.prepare(48000.0, 512, 1);
    a.paramsRef() = adapterPatch();
    a.commitParameters();

    const uint8_t stream[] = {
        0xC0, 5,                    // programme change: ignored, 2 bytes
        0x90, 60, 100,              // note on
        0xB0, 64, 127,              // sustain down
        0xE0, 0, 80,                // bend up
        0x80, 60, 0,                // note off
        0xB0, 64, 0,                // sustain up
    };
    a.handleMidiBuffer(stream, static_cast<int>(sizeof(stream)));
    CHECK(a.lastMidiNote() == 60);
    // 0xE0, 0, 80 -> pitch value 10240 -> (10240-8192)/8192 = +0.25 of full
    // scale -> +0.5 semitones at the default +/-2 semitone range.
    CHECK_NEAR(a.pitchBendSemitones(), 0.5, 0.01);

    std::vector<float> buf(9600);
    CHECK_LT(steadyRms(a, buf, 9600, 4800), 1e-3);    // note off + pedal up = silence
    CHECK(!a.engine().gate());

    // A truncated message must not read past the end of the buffer.
    const uint8_t truncated[] = { 0x90, 60 };
    a.handleMidiBuffer(truncated, 2);
    CHECK(!a.engine().gate());
    a.handleMidiBuffer(nullptr, 0);
    CHECK(!a.engine().gate());
}

// A note that starts and ends between two rendered samples must still be played
// and released — hosts deliver a block's MIDI *before* rendering that block, so
// this is the normal case for fast playing at small buffer sizes.  Before the
// gate-event queue was added, the envelope was left running with the gate
// already low and the voice never went quiet (found by tools/adapter_probe.cpp).
SH101_TEST(adapter_note_inside_one_block_is_released) {
    SH101HostAdapter a;
    a.prepare(48000.0, 512, 1);
    a.paramsRef() = adapterPatch();
    a.commitParameters();

    std::vector<float> buf(9600);

    // Note on and off before a single sample is rendered.
    a.handleMidiMessage(0x90, 60, 100);
    a.handleMidiMessage(0x80, 60, 0);
    CHECK_LT(steadyRms(a, buf, 9600, 4800), 1e-3);
    CHECK(!a.engine().gate());

    // The voice must still work afterwards.
    a.handleMidiMessage(0x90, 62, 100);
    a.renderBlock(buf.data(), 4800);
    CHECK_GT(rmsOf(buf), 0.01);
    a.handleMidiMessage(0x80, 62, 0);
    CHECK_LT(steadyRms(a, buf, 9600, 4800), 1e-3);

    // A note shorter than a normal host block, rendered partly: the envelope
    // starts, then releases, and nothing is left running.
    a.handleMidiMessage(0x90, 64, 100);
    a.renderBlock(buf.data(), 32);
    a.handleMidiMessage(0x80, 64, 0);
    CHECK_LT(steadyRms(a, buf, 9600, 4800), 1e-3);
    CHECK_NEAR(a.engine().envelopeValue(), 0.0, 1e-6);

    // Several notes inside one block: only the last one may remain gated.
    a.handleMidiMessage(0x90, 60, 100);
    a.handleMidiMessage(0x80, 60, 0);
    a.handleMidiMessage(0x90, 65, 100);
    a.handleMidiMessage(0x80, 65, 0);
    a.handleMidiMessage(0x90, 69, 100);
    a.renderBlock(buf.data(), 4800);
    CHECK_GT(rmsOf(buf), 0.01);
    CHECK(a.engine().currentNote() == 69);
    a.handleMidiMessage(0x80, 69, 0);
    CHECK_LT(steadyRms(a, buf, 9600, 4800), 1e-3);
}

// Host clock sync is opt-in: the free-running arpeggiator keeps working, and
// with sync enabled the steps follow the host's ticks.
SH101_TEST(adapter_host_clock_is_opt_in) {
    SH101HostAdapter a;
    a.prepare(48000.0, 512, 1);
    SH101Params p = adapterPatch();
    p.arpOn = true;
    p.arpRate = 20.0;
    p.arpMode = Arpeggiator::Up;
    p.arpOctaves = 1;
    p.decay = 0.05;
    p.sustain = 0.0;
    a.paramsRef() = p;
    a.commitParameters();

    // Free-running: notes advance on the internal clock.
    a.handleMidiMessage(0x90, 48, 100);
    a.handleMidiMessage(0x90, 52, 100);
    std::vector<float> buf(24000);
    a.renderBlock(buf.data(), 24000);
    CHECK_GT(rmsOf(buf), 0.001);

    // With the host clock selected, steps only advance on ticks.
    a.setArpSyncToHost(true);
    a.renderBlock(buf.data(), 4800);
    const int noteBefore = a.engine().currentNote();
    a.hostClockTick();
    a.renderBlock(buf.data(), 48);
    const int noteAfter = a.engine().currentNote();
    CHECK(noteBefore != noteAfter);
}
