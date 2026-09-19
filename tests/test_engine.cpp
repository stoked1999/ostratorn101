// Engine-level integration validation.
//
// Validation list from the brief (SONIC FIDELITY FIRST / "Validation
// requirement") plus the automated-test list: deterministic output in test mode,
// no NaN/Inf/denormal failures, host sample rates and block sizes, bounded
// output, parameter smoothing, no heap allocation in the audio callback.
#include <atomic>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <new>
#include <type_traits>
#include <vector>

#include "TestFramework.h"
#include "TestHelpers.h"
#include "sh101/SH101Engine.h"

using namespace sh101;
using namespace sh101test;

// ---- Heap-allocation sentinel ---------------------------------------------
// Replacing the global allocator lets the test prove that rendering does not
// allocate: the brief requires "no heap allocations in the audio callback".
//
// The counter must be a namespace-scope object with constant initialisation and
// no guard variable: the first operator new call can come from another
// translation unit's static initialiser, and anything with a function-local
// static (or any lazy initialisation) inside the allocation path would re-enter
// its own initialisation and blow the stack before main().
//
// SH101_ENABLE_ALLOC_SENTINEL can be set to 0 to fall back to a plain counter
// (useful when a toolchain cannot tolerate a replaced global allocator at all).
// With it enabled the suite reports the bytes allocated during rendering, which
// must be zero.
#ifndef SH101_ENABLE_ALLOC_SENTINEL
#define SH101_ENABLE_ALLOC_SENTINEL 1
#endif

#if SH101_ENABLE_ALLOC_SENTINEL
namespace {
std::atomic<long long> g_allocBytes{ 0 };
} // namespace
#else
namespace {
long long g_allocBytes = 0;   // never written, never read in this mode
} // namespace
#endif

#if SH101_ENABLE_ALLOC_SENTINEL
void* operator new(size_t n) {
    g_allocBytes += static_cast<long long>(n);
    void* p = std::malloc(n ? n : 1);
    if (!p) throw std::bad_alloc();
    return p;
}
void* operator new[](size_t n) {
    g_allocBytes += static_cast<long long>(n);
    void* p = std::malloc(n ? n : 1);
    if (!p) throw std::bad_alloc();
    return p;
}
void operator delete(void* p) noexcept { std::free(p); }
void operator delete[](void* p) noexcept { std::free(p); }
void operator delete(void* p, size_t) noexcept { std::free(p); }
void operator delete[](void* p, size_t) noexcept { std::free(p); }
#endif

namespace {

SH101Params fullPatch() {
    SH101Params p;
    p.sawLevel = 0.8;
    p.pulseLevel = 0.6;
    p.subLevel = 0.5;
    p.subMode = 0;
    p.noiseLevel = 0.1;
    p.cutoff = 1500.0;
    p.resonance = 0.4;
    p.filterEnvAmt = 2.0;
    p.filterModAmt = 0.3;
    p.keyTrack = 0.4;
    p.attack = 0.005;
    p.decay = 0.4;
    p.sustain = 0.5;
    p.release = 0.2;
    p.vcoModDep = 0.05;
    p.pulseWidth = 0.4;
    p.pwmSource = 2;
    p.pwmAmount = 0.4;
    p.lfoRate = 4.0;
    p.lfoWave = SH101LFO::Triangle;
    p.volume = 0.8;
    return p;
}

std::vector<float> render(SH101Engine& e, int samples) {
    std::vector<float> out(static_cast<size_t>(samples));
    e.renderBlock(out.data(), samples);
    return out;
}

double rmsF(const std::vector<float>& x) {
    double s = 0.0;
    for (float v : x) s += static_cast<double>(v) * v;
    return std::sqrt(s / static_cast<double>(x.size()));
}

bool allFiniteF(const std::vector<float>& x) {
    for (float v : x) {
        if (!std::isfinite(v)) return false;
    }
    return true;
}

} // namespace

// Deterministic output in test mode: identical seeds and identical parameters
// must give bit-identical audio.
SH101_TEST(engine_render_is_deterministic) {
    const int n = 9600;
    SH101Engine a, b;
    a.prepare(48000.0, 2);
    b.prepare(48000.0, 2);
    a.setDeterministicTestMode(true, 0xC0FFEEu);
    b.setDeterministicTestMode(true, 0xC0FFEEu);
    const SH101Params p = fullPatch();
    a.setParams(p);
    b.setParams(p);
    a.noteOn(48, 1.0);
    a.noteOn(52, 1.0);
    b.noteOn(48, 1.0);
    b.noteOn(52, 1.0);

    const std::vector<float> xa = render(a, n);
    const std::vector<float> xb = render(b, n);
    CHECK(std::memcmp(xa.data(), xb.data(), sizeof(float) * static_cast<size_t>(n)) == 0);

    // Diferent seeds must differ, which proves the noise paths use the seed.
    SH101Engine c;
    c.prepare(48000.0, 2);
    c.setDeterministicTestMode(true, 0xBEEFu);
    c.setParams(p);
    c.noteOn(48, 1.0);
    c.noteOn(52, 1.0);
    const std::vector<float> xc = render(c, n);
    CHECK(std::memcmp(xa.data(), xc.data(), sizeof(float) * static_cast<size_t>(n)) != 0);
}

// No gate: no sound.  (Only the VCA's small feedthrough term remains.)
SH101_TEST(engine_is_silent_without_a_gate) {
    SH101Engine engine;
    engine.prepare(48000.0, 2);
    SH101Params p = fullPatch();
    p.resonance = 0.0;
    p.noiseLevel = 0.0;
    engine.setParams(p);
    const std::vector<float> out = render(engine, 24000);
    CHECK_LT(peakAbs(std::vector<double>(out.begin(), out.end())), 1.0e-3);
    CHECK(engine.safetyResetCount() == 0);
}

// A note produces audio; releasing it and letting the release finish returns the
// voice to silence.
SH101_TEST(engine_note_on_and_off_envelope_behaviour) {
    SH101Engine engine;
    engine.prepare(48000.0, 2);
    SH101Params p = fullPatch();
    p.noiseLevel = 0.0;
    p.vcoModDep = 0.0;
    engine.setParams(p);
    engine.noteOn(48, 1.0);
    const std::vector<float> on = render(engine, 12000);
    CHECK_GT(rmsF(on), 0.01);

    engine.noteOff(48);
    render(engine, static_cast<int>(48000.0 * (p.release + 0.2)));
    const std::vector<float> off = render(engine, 4800);
    std::printf("      rms with note %.4f, after release %.6f\n", rmsF(on), rmsF(off));
    CHECK_LT(rmsF(off), 1.0e-3);
    CHECK(!engine.gate());
}

// Extreme settings, every supported sample rate and oversampling factor: the
// output must stay finite, bounded, and must never trip the safety reset.
SH101_TEST(engine_host_rates_oversampling_and_extremes) {
    for (double sr : { 44100.0, 48000.0, 88200.0, 96000.0 }) {
        for (int os : { 1, 2, 4 }) {
            SH101Engine engine;
            engine.prepare(sr, os);
            SH101Params p = fullPatch();
            p.sawLevel = 1.0;
            p.pulseLevel = 1.0;
            p.subLevel = 1.0;
            p.noiseLevel = 1.0;
            p.resonance = 1.0;          // self-oscillation
            p.cutoff = 12000.0;         // near Nyquist
            p.filterEnvAmt = 6.0;
            p.pwmAmount = 1.0;
            p.vcoModDep = 1.0;
            p.lfoRate = 30.0;
            p.lfoWave = SH101LFO::Noise;
            p.attack = 0.0015;
            p.decay = 0.002;
            p.release = 0.002;
            engine.setParams(p);
            engine.noteOn(96, 1.0);
            std::vector<float> out = render(engine, static_cast<int>(sr * 0.3));
            engine.noteOn(108, 1.0);
            std::vector<float> out2 = render(engine, static_cast<int>(sr * 0.3));
            out.insert(out.end(), out2.begin(), out2.end());

            CHECK(allFiniteF(out));
            CHECK_LT(peakAbs(std::vector<double>(out.begin(), out.end())), 2.5);
            CHECK(engine.safetyResetCount() == 0);
        }
    }
}

// Block-size independence: rendering in one block and one sample at a time must
// produce identical audio, so host block size cannot change the sound.
SH101_TEST(engine_block_size_independence) {
    const int n = 1024;
    SH101Engine a, b;
    a.prepare(48000.0, 2);
    b.prepare(48000.0, 2);
    const SH101Params p = fullPatch();
    a.setParams(p);
    b.setParams(p);

    float block[n];
    float single[n];
    a.renderBlock(block, n);
    for (int i = 0; i < n; ++i) single[i] = b.renderSample();
    CHECK(std::memcmp(block, single, sizeof(float) * static_cast<size_t>(n)) == 0);

    // A silent block and a block with a note-on in the middle behave the same.
    SH101Engine c;
    c.prepare(48000.0, 2);
    c.setParams(p);
    const std::vector<float> pre = render(c, 128);
    c.noteOn(60, 1.0);
    const std::vector<float> post = render(c, 128);
    CHECK_LT(rmsF(pre), 1.0e-3);
    CHECK_GT(rmsF(post), 1.0e-4);
}

// Pitch bend maps to the bender's range in semitones.
SH101_TEST(engine_pitch_bend_maps_to_semitones) {
    SH101Engine engine;
    engine.prepare(48000.0, 1);
    // The bend ratio is asserted to 0.1%, which is finer than the analog pitch
    // tolerance of a real instrument (AnalogVariation: up to ~0.23%).  This test
    // therefore measures the calibrated core; the tolerance has its own tests in
    // tests/test_analog.cpp.
    engine.setAnalogVariationEnabled(false);
    SH101Params p = fullPatch();
    p.sawLevel = 1.0;
    p.pulseLevel = 0.0;
    p.subLevel = 0.0;
    p.noiseLevel = 0.0;
    p.vcoModDep = 0.0;
    p.pulseWidth = 0.5;
    engine.setParams(p);
    engine.noteOn(60, 1.0);
    render(engine, 4800);
    const double base = engine.oscillatorHz();
    CHECK_NEAR(base, cvToFrequency(midiNoteToCV(60)), 1.0);

    engine.setPitchBendSemitones(2.0);
    render(engine, 4800);
    CHECK_NEAR(engine.oscillatorHz(), base * std::pow(2.0, 2.0 / 12.0), base * 0.001);

    engine.setPitchBendSemitones(-2.0);
    render(engine, 4800);
    CHECK_NEAR(engine.oscillatorHz(), base * std::pow(2.0, -2.0 / 12.0), base * 0.001);

    engine.setPitchBendSemitones(0.0);
    engine.allNotesOff();
    render(engine, static_cast<int>(48000.0 * (p.release + 0.3)));
    CHECK_LT(std::fabs(engine.lastOutput()), 1.0e-3);
}

// Audio-rate controls are smoothed: a step change in volume must not create a
// discontinuity.  The test isolates the parameter step by comparing the largest
// sample-to-sample step during the change against the settled steps at the old
// and new values — without smoothing, the change sample would jump by the whole
// level difference.
SH101_TEST(engine_parameter_smoothing_has_no_steps) {
    SH101Engine engine;
    engine.prepare(48000.0, 2);
    SH101Params p = fullPatch();
    p.noiseLevel = 0.0;
    p.resonance = 0.2;
    p.vcoModDep = 0.0;
    p.volume = 0.2;
    engine.setParams(p);
    engine.noteOn(60, 1.0);
    render(engine, 4800);

    auto maxStep = [](const std::vector<float>& x) {
        double m = 0.0;
        for (size_t i = 1; i < x.size(); ++i) {
            m = std::max(m, std::fabs(static_cast<double>(x[i]) - x[i - 1]));
        }
        return m;
    };

    const double stepLow = maxStep(render(engine, 2000));

    // Step the volume up; measure only the smoothing window (2 ms).
    p.volume = 1.0;
    engine.setParams(p);
    const double stepChange = maxStep(render(engine, 96));

    // Settle and measure the new steady state.
    render(engine, 9600);
    const double stepHigh = maxStep(render(engine, 2000));

    std::printf("      max sample step: at 0.2 = %.4f, at 1.0 = %.4f, during change = %.4f\n",
                stepLow, stepHigh, stepChange);
    CHECK_GT(stepHigh, stepLow);                          // the change took effect
    // 3x the settled step allows for the smoothing ramp riding on a waveform
    // whose own slew scales with the level; an unsmoothed jump would be tens of
    // times larger than this.
    CHECK_LT(stepChange, stepHigh * 3.0);
}

// Rendering must not allocate on the heap (allocator sentinel above; see the
// SH101_ENABLE_ALLOC_SENTINEL note for the toolchains where the sentinel cannot
// be linked in).
SH101_TEST(engine_render_does_not_allocate) {
    SH101Engine engine;
    engine.prepare(48000.0, 4);
    SH101Params p = fullPatch();
    engine.setParams(p);
    engine.noteOn(60, 1.0);
    render(engine, 1024);   // warm up (oversampler/filter state is prepared already)

#if SH101_ENABLE_ALLOC_SENTINEL
    const long long before = g_allocBytes.load();
#endif
    float block[512];
    for (int i = 0; i < 32; ++i) {
        if (i % 8 == 0) engine.noteOn(48 + i, 1.0);
        engine.renderBlock(block, 512);
    }
#if SH101_ENABLE_ALLOC_SENTINEL
    const long long after = g_allocBytes.load();
    std::printf("      heap bytes allocated during 32 blocks: %lld\n", after - before);
    CHECK(after == before);
#else
    std::printf("      allocator sentinel disabled on this toolchain "
                "(see tools/check_no_alloc.py for the static check)\n");
#endif
}

// Structural guarantee that backs the allocation check in both modes: the DSP
// classes are plain value types with fixed-size storage, so no member can own
// heap memory.  Adding a std::vector/std::string/std::function member to any of
// them breaks this test.
SH101_TEST(engine_dsp_types_are_heap_free_value_types) {
    static_assert(std::is_trivially_copyable<SH101VCF>::value, "SH101VCF must not own heap memory");
    static_assert(std::is_trivially_copyable<SH101VCO>::value, "SH101VCO must not own heap memory");
    static_assert(std::is_trivially_copyable<SubOscDivider>::value, "SubOscDivider must not own heap memory");
    static_assert(std::is_trivially_copyable<SourceMixer>::value, "SourceMixer must not own heap memory");
    static_assert(std::is_trivially_copyable<SH101VCA>::value, "SH101VCA must not own heap memory");
    static_assert(std::is_trivially_copyable<SH101Envelope>::value, "SH101Envelope must not own heap memory");
    static_assert(std::is_trivially_copyable<SH101LFO>::value, "SH101LFO must not own heap memory");
    static_assert(std::is_trivially_copyable<OutputStage>::value, "OutputStage must not own heap memory");
    static_assert(std::is_trivially_copyable<Portamento>::value, "Portamento must not own heap memory");
    static_assert(std::is_trivially_copyable<VoiceController>::value, "VoiceController must not own heap memory");
    static_assert(std::is_trivially_copyable<Arpeggiator>::value, "Arpeggiator must not own heap memory");
    static_assert(std::is_trivially_copyable<StepSequencer>::value, "StepSequencer must not own heap memory");
    static_assert(std::is_trivially_copyable<Smoother>::value, "Smoother must not own heap memory");
    CHECK(sizeof(SH101Engine) > 0);
}

// Gate transitions that happen between rendered samples must not be lost: with
// the note released before any audio is rendered, the envelope has to end up
// idle rather than stuck at sustain (this is the host-block case).
SH101_TEST(engine_gate_edges_between_samples_are_not_lost) {
    SH101Engine engine;
    engine.prepare(48000.0, 1);
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
    p.envTrigger = SH101Envelope::GateAndTrig;
    engine.setParams(p);

    engine.noteOn(60, 1.0);
    engine.noteOff(60);                      // both before rendering anything
    std::vector<float> tail = render(engine, 48000);
    CHECK_NEAR(engine.envelopeValue(), 0.0, 1e-6);
    CHECK(!engine.gate());
    CHECK_LT(rmsF(std::vector<float>(tail.end() - 9600, tail.end())), 1e-3);

    // The same with GATE (non-retriggering) mode.
    p.envTrigger = SH101Envelope::GateOnly;
    engine.setParams(p);
    engine.noteOn(62, 1.0);
    engine.noteOff(62);
    render(engine, 48000);
    CHECK_NEAR(engine.envelopeValue(), 0.0, 1e-6);

    // And in GATE mode a genuinely held note still sustains.
    engine.noteOn(64, 1.0);
    render(engine, 24000);
    CHECK_GT(engine.envelopeValue(), 0.9);
    engine.noteOff(64);
    render(engine, 24000);
    CHECK_NEAR(engine.envelopeValue(), 0.0, 1e-6);
}

// Long run with everything at once: no NaN, no drift into denormal/instability,
// and the voice still responds after minutes of rendered audio.
SH101_TEST(engine_long_run_stability) {
    SH101Engine engine;
    engine.prepare(96000.0, 4);
    SH101Params p = fullPatch();
    p.resonance = 1.0;
    p.noiseLevel = 0.5;
    p.cutoff = 300.0;
    p.keyTrack = 1.0;
    engine.setParams(p);

    double worstPeak = 0.0;
    for (int note = 36; note < 60; ++note) {
        engine.noteOn(note, 1.0);
        const std::vector<float> out = render(engine, 9600);
        CHECK(allFiniteF(out));
        worstPeak = std::max(worstPeak, peakAbs(std::vector<double>(out.begin(), out.end())));
        engine.noteOff(note);
        render(engine, 2400);
    }
    std::printf("      worst peak over 24 notes: %.3f, safety resets: %d\n", worstPeak,
                engine.safetyResetCount());
    CHECK_LT(worstPeak, 2.5);
    CHECK(engine.safetyResetCount() == 0);
}
