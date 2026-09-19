// VCF validation: cutoff tracking, four-pole slope, resonance, self-oscillation
// and envelope/LFO depth.
//
// Validation list from the brief: "filter cutoff tracking", "resonance amount
// and self-oscillation", "filter FM/envelope depth".
//
// [APPROX] The filter's stage/feedback coefficients are not yet value-matched to
// the schematic.  These tests therefore assert topology-level behaviour (four
// poles, cutoff tracking, self-oscillation tracking, stability) plus the
// measured numbers, rather than pretending to match a specific measured unit.
#include <cstdio>
#include <vector>

#include "TestFramework.h"
#include "TestHelpers.h"
#include "sh101/SH101Engine.h"
#include "sh101/SH101VCF.h"

using namespace sh101;
using namespace sh101test;

namespace {

std::vector<double> runFilter(SH101VCF& f, double inputFreq, double sr, int n, double amp = 0.05) {
    std::vector<double> out;
    out.reserve(n);
    double phase = 0.0;
    for (int i = 0; i < n; ++i) {
        const double x = amp * std::sin(kTwoPi * phase);
        phase += inputFreq / sr;
        if (phase >= 1.0) phase -= 1.0;
        out.push_back(f.process(x));
    }
    return out;
}

double steadyRms(const std::vector<double>& x, int skip) {
    if (static_cast<int>(x.size()) <= skip) return 0.0;
    return rms(std::vector<double>(x.begin() + skip, x.end()));
}

// Self-oscillation frequency of the filter alone, with a tiny impulse to start
// the loop (a real board starts from thermal noise; the tests stay deterministic).
double measureSelfOsc(SH101VCF& f, double sr, int n) {
    std::vector<double> out;
    out.reserve(n);
    for (int i = 0; i < n; ++i) {
        const double x = (i == 0) ? 1.0e-3 : 0.0;
        out.push_back(f.process(x));
    }
    return zeroCrossFrequency(std::vector<double>(out.begin() + n / 4, out.end()), sr);
}

} // namespace

// A four-pole low-pass attenuates a sine at the corner by about -12 dB relative
// to the passband (each OTA stage contributes -3 dB), and by far more two
// octaves up.  This is the "four poles, not a biquad" evidence.
SH101_TEST(filter_is_four_pole_with_cutoff_tracking) {
    const double sr = 48000.0;
    const double fc = 1000.0;
    SH101VCF f;
    f.prepare(sr, Calibration{}, 2);
    f.setResonance(0.0);
    f.setCutoffHz(fc);

    const int n = 24000;
    const double pas = steadyRms(runFilter(f, 50.0, sr, n), n / 4);
    f.reset();
    const double atFc = steadyRms(runFilter(f, fc, sr, n), n / 4);
    f.reset();
    const double at4Fc = steadyRms(runFilter(f, 4.0 * fc, sr, n), n / 4);

    const double atFcDb = db(atFc / pas);
    const double at4FcDb = db(at4Fc / pas);
    std::printf("      response at fc: %.1f dB, at 4*fc: %.1f dB\n", atFcDb, at4FcDb);

    CHECK_NEAR(atFcDb, -12.0, 3.0);      // 4 stages x -3 dB
    CHECK_LT(at4FcDb, -40.0);            // steep 24 dB/oct class roll-off
    CHECK_NEAR(f.cutoffHz(), fc, 1.0);
}

// Cutoff tracking across the documented range: the self-oscillation frequency is
// the corner frequency (this is exactly how the service notes say to set VCF
// tracking), so it is measured here and compared with the commanded cutoff.
SH101_TEST(filter_self_oscillation_tracks_cutoff) {
    const double sr = 48000.0;
    for (double fc : { 100.0, 400.0, 1200.0, 4000.0 }) {
        SH101VCF f;
        f.prepare(sr, Calibration{}, 2);
        f.setResonance(1.0);
        f.setCutoffHz(fc);
        const double measured = measureSelfOsc(f, sr, static_cast<int>(sr * 1.0));
        std::printf("      commanded %7.1f Hz -> self-oscillation %7.1f Hz\n", fc, measured);
        CHECK_GT(f.loopGain(), 1.0);                 // must be able to oscillate
        CHECK_NEAR(measured, fc, fc * 0.20);         // [APPROX] tanh lag shifts it slightly
        CHECK_LT(peakAbs(runFilter(f, fc, sr, 4800)), 4.0);  // bounded, not divergent
    }
}

// Resonance must audibly increase the peak at the corner and stay stable at the
// maximum setting, including without oversampling (the worst case for a
// nonlinear feedback loop).
SH101_TEST(filter_resonance_is_monotonic_and_stable_at_maximum) {
    const double sr = 48000.0;
    const double fc = 800.0;
    double previousPeak = 0.0;
    for (int os : { 1, 2, 4 }) {
        double previousResGain = 0.0;
        for (double res : { 0.0, 0.25, 0.5, 0.75, 1.0 }) {
            SH101VCF f;
            f.prepare(sr, Calibration{}, os);
            f.setResonance(res);
            f.setCutoffHz(fc);
            const int n = 24000;
            std::vector<double> out = runFilter(f, fc, sr, n);
            const double pk = steadyRms(out, n / 4);
            CHECK(allFinite(out));
            CHECK_LT(peakAbs(out), 5.0);
            if (res > 0.0) {
                CHECK_GT(pk, previousResGain);   // more resonance = bigger corner peak
            }
            previousResGain = pk;
            previousPeak = std::max(previousPeak, pk);
        }
    }
    CHECK_GT(previousPeak, 0.0);
}

// Filter modulation depth: the engine's cutoff CV must land where the control
// maths says it should, for envelope and LFO modulation.
SH101_TEST(filter_cutoff_cv_depth_from_envelope_and_lfo) {
    const double sr = 48000.0;
    SH101Params p;
    p.sawLevel = 1.0;
    p.pulseLevel = 0.0;
    p.cutoff = 500.0;
    p.resonance = 0.0;
    p.attack = 0.01;
    p.decay = 10.0;
    p.sustain = 1.0;
    p.release = 0.1;
    p.filterEnvAmt = 0.0;
    p.filterModAmt = 0.0;
    p.keyTrack = 0.0;

    SH101Engine engine;
    engine.prepare(sr, 2);

    // No modulation: the commanded cutoff is the manual setting.
    engine.setParams(p);
    engine.noteOn(60, 1.0);
    for (int i = 0; i < static_cast<int>(sr * 0.2); ++i) engine.renderSample();
    CHECK_NEAR(engine.cutoffHz(), p.cutoff, p.cutoff * 0.02);

    // Envelope depth of +3 octaves at full sustain.
    p.filterEnvAmt = 3.0;
    engine.setParams(p);
    for (int i = 0; i < static_cast<int>(sr * 0.2); ++i) engine.renderSample();
    CHECK_NEAR(engine.cutoffHz(), p.cutoff * 8.0, p.cutoff * 8.0 * 0.03);

    // LFO depth of +2 octaves, measured at the modulator's positive peak.  The
    // first 60 ms are skipped: the smoothers still carry the previous stage's
    // values, and this test is about steady-state modulation depth.
    p.filterEnvAmt = 0.0;
    p.filterModAmt = 2.0;
    p.lfoRate = 1.0;
    p.lfoWave = SH101LFO::Square;
    engine.setParams(p);
    for (int i = 0; i < static_cast<int>(sr * 0.06); ++i) engine.renderSample();
    double maxCutoff = 0.0;
    double minCutoff = 1.0e9;
    for (int i = 0; i < static_cast<int>(sr * 1.2); ++i) {
        engine.renderSample();
        maxCutoff = std::max(maxCutoff, engine.cutoffHz());
        minCutoff = std::min(minCutoff, engine.cutoffHz());
    }
    std::printf("      cutoff with +/-2 octave LFO: %.1f .. %.1f Hz\n", minCutoff, maxCutoff);
    CHECK_NEAR(maxCutoff, p.cutoff * 4.0, p.cutoff * 4.0 * 0.05);
    CHECK_NEAR(minCutoff, p.cutoff / 4.0, p.cutoff / 4.0 * 0.05);
    engine.noteOff(60);
}

// Key follow: with tracking at maximum, the self-oscillation pitch must follow
// the keyboard at 1 V/oct (this is the service calibration condition).
SH101_TEST(filter_self_osc_tracks_keyboard_at_full_key_follow) {
    const double sr = 48000.0;
    SH101Params p;
    p.sawLevel = 0.0;
    p.pulseLevel = 0.0;
    p.subLevel = 0.0;
    p.noiseLevel = 0.05;      // seeds the loop the way thermal noise does
    p.cutoff = 200.0;         // cutoff at C4 (MIDI 60)
    p.resonance = 1.0;
    p.keyTrack = 1.0;
    p.attack = 0.005;
    p.decay = 20.0;
    p.sustain = 1.0;
    p.release = 0.1;
    p.filterEnvAmt = 0.0;

    SH101Engine engine;
    engine.prepare(sr, 2);
    engine.setParams(p);

    auto measureNote = [&](int note) {
        engine.reset();
        engine.setParams(p);
        engine.noteOn(note, 1.0);
        std::vector<double> out;
        const int n = static_cast<int>(sr * 1.5);
        out.reserve(n);
        for (int i = 0; i < n; ++i) out.push_back(engine.renderSample());
        engine.noteOff(note);
        return zeroCrossFrequency(std::vector<double>(out.begin() + n / 2, out.end()), sr);
    };

    const double f60 = measureNote(60);
    const double f72 = measureNote(72);   // one octave up
    const double f48 = measureNote(48);   // one octave down
    std::printf("      self-oscillation: MIDI 48 %.1f Hz, 60 %.1f Hz, 72 %.1f Hz\n", f48, f60, f72);

    CHECK_GT(f60, 50.0);
    CHECK_NEAR(f72 / f60, 2.0, 0.15);
    CHECK_NEAR(f48 / f60, 0.5, 0.15);
}
