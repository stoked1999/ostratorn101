// Envelope and LFO validation.
//
// Validation list from the brief: "envelope attack/decay/release timing and
// curves", "LFO rates and waveform timing".
//
// The documented envelope ranges (A 1.5 ms..4 s, D 2 ms..10 s, R 2 ms..10 s)
// are asserted through the normalized parameter mapping in Params.h, which is
// what a host would actually drive.
#include <cstdio>
#include <tuple>
#include <vector>

#include "TestFramework.h"
#include "TestHelpers.h"
#include "sh101/Params.h"
#include "sh101/SH101Envelope.h"
#include "sh101/SH101LFO.h"

using namespace sh101;
using namespace sh101test;

namespace {

struct EnvTrace {
    std::vector<double> value;
    int samplesToPeak = -1;
};

EnvTrace traceEnvelope(SH101Envelope& env, int n) {
    EnvTrace t;
    t.value.reserve(n);
    for (int i = 0; i < n; ++i) {
        const double v = env.process();
        t.value.push_back(v);
        if (t.samplesToPeak < 0 && v >= 0.999) t.samplesToPeak = i;
    }
    return t;
}

} // namespace

// Attack times across the documented range, measured as the time to reach 99%
// of full scale.
SH101_TEST(envelope_attack_times_match_documented_range) {
    const double sr = 48000.0;
    for (double a : { 0.0015, 0.01, 0.1, 1.0, 4.0 }) {
        SH101Envelope env;
        env.prepare(sr);
        env.setSustain(1.0);
        env.setAttack(a);
        env.setDecay(1.0);
        env.setRelease(0.1);
        env.setTriggerMode(SH101Envelope::GateAndTrig);
        env.noteOn(true);
        const int n = static_cast<int>(std::min(10.0, a * 3.0 + 0.05) * sr);
        const EnvTrace t = traceEnvelope(env, n);
        CHECK(t.samplesToPeak > 0);
        const double measured = static_cast<double>(t.samplesToPeak) / sr;
        const double tol = std::max(0.0015, a * 0.12);
        std::printf("      attack %.4f s -> measured %.4f s\n", a, measured);
        CHECK_NEAR(measured, a, tol);
    }
}

// The brief forbids linear ramps: the analog attack charges towards a target
// above the threshold, so the curve is convex and is already well above a linear
// ramp at half the attack time.
SH101_TEST(envelope_curves_are_exponential_not_linear) {
    const double sr = 48000.0;
    SH101Envelope env;
    env.prepare(sr);
    env.setAttack(0.2);
    env.setSustain(1.0);
    env.setTriggerMode(SH101Envelope::GateAndTrig);
    env.noteOn(true);

    std::vector<double> v;
    const int n = static_cast<int>(0.2 * sr);
    for (int i = 0; i < n; ++i) v.push_back(env.process());

    const double atHalf = v[v.size() / 2];
    std::printf("      attack value at 50%% of the attack time: %.3f (linear would be 0.500)\n",
                atHalf);
    CHECK_GT(atHalf, 0.55);
    CHECK_LT(atHalf, 0.80);

    // Decay/release discharge towards their targets, i.e. they start fast and
    // slow down: the value after one decay time constant is well past halfway.
    SH101Envelope env2;
    env2.prepare(sr);
    env2.setAttack(0.0015);
    env2.setDecay(1.0);
    env2.setSustain(0.0);
    env2.setTriggerMode(SH101Envelope::GateAndTrig);
    env2.noteOn(true);
    std::vector<double> d;
    const int n2 = static_cast<int>(1.0 * sr);
    for (int i = 0; i < n2; ++i) d.push_back(env2.process());
    // At half the decay time the exponential has already fallen below the
    // linear midpoint (linear would be 0.5).
    const double decayHalf = d[d.size() / 2];
    std::printf("      decay value at 50%% of the decay time: %.3f (linear would be 0.500)\n",
                decayHalf);
    CHECK_LT(decayHalf, 0.35);
}

// Decay and release timing, measured from full scale down to 1% as documented
// in Calibration.h.
SH101_TEST(envelope_decay_and_release_times) {
    const double sr = 48000.0;
    for (double dec : { 0.002, 0.05, 0.5, 5.0 }) {
        SH101Envelope env;
        env.prepare(sr);
        env.setAttack(0.0015);
        env.setDecay(dec);
        env.setSustain(0.0);
        env.setTriggerMode(SH101Envelope::GateAndTrig);
        env.noteOn(true);
        const int n = static_cast<int>(std::min(20.0, dec * 3.0 + 0.05) * sr);
        const EnvTrace t = traceEnvelope(env, n);
        const double measured = timeToFall(t.value, sr, 0.01, static_cast<size_t>(t.samplesToPeak));
        CHECK_GT(measured, 0.0);
        std::printf("      decay  %.4f s -> measured %.4f s\n", dec, measured);
        CHECK_NEAR(measured, dec, std::max(0.003, dec * 0.12));
    }

    for (double rel : { 0.002, 0.05, 0.5, 5.0 }) {
        SH101Envelope env;
        env.prepare(sr);
        env.setAttack(0.0015);
        env.setDecay(0.002);
        env.setSustain(1.0);
        env.setRelease(rel);
        env.setTriggerMode(SH101Envelope::GateAndTrig);
        env.noteOn(true);
        // Let it reach sustain/peak.
        for (int i = 0; i < static_cast<int>(0.01 * sr); ++i) env.process();
        env.noteOff();
        std::vector<double> v;
        const int n = static_cast<int>(std::min(20.0, rel * 3.0 + 0.05) * sr);
        for (int i = 0; i < n; ++i) v.push_back(env.process());
        const double measured = timeToFall(v, sr, 0.01);
        CHECK_GT(measured, 0.0);
        std::printf("      release %.4f s -> measured %.4f s\n", rel, measured);
        CHECK_NEAR(measured, rel, std::max(0.003, rel * 0.12));
    }
}

// Sustain level and the documented parameter ranges through the normalized
// parameter mapping.
SH101_TEST(envelope_sustain_level_and_parameter_ranges) {
    SH101Params p;
    applyNormalized(p, pAttack, 0.0);
    CHECK_NEAR(p.attack, 0.0015, 1e-9);
    applyNormalized(p, pAttack, 1.0);
    CHECK_NEAR(p.attack, 4.0, 1e-9);
    applyNormalized(p, pDecay, 0.0);
    CHECK_NEAR(p.decay, 0.002, 1e-9);
    applyNormalized(p, pDecay, 1.0);
    CHECK_NEAR(p.decay, 10.0, 1e-9);
    applyNormalized(p, pRelease, 0.0);
    CHECK_NEAR(p.release, 0.002, 1e-9);
    applyNormalized(p, pRelease, 1.0);
    CHECK_NEAR(p.release, 10.0, 1e-9);

    SH101Envelope env;
    env.prepare(48000.0);
    env.setAttack(0.002);
    env.setDecay(0.05);
    env.setSustain(0.4);
    env.setRelease(0.05);
    env.setTriggerMode(SH101Envelope::GateAndTrig);
    env.noteOn(true);
    for (int i = 0; i < 48000; ++i) env.process();
    CHECK_NEAR(env.value(), 0.4, 0.01);
    CHECK(env.stage() == SH101Envelope::Sustain);
}

// Trigger modes: GATE+TRIG retriggers on every note, GATE only starts a new
// attack when the envelope is not already running, LFO lets the modulator drive
// the gate.
SH101_TEST(envelope_trigger_modes) {
    const double sr = 48000.0;

    // GATE+TRIG vs GATE: with a short attack, a long decay and no sustain, a
    // legato note change either restarts the attack (GATE+TRIG) or leaves the
    // envelope running (GATE).  That difference is what the flag controls.
    auto runLegatoNoteChange = [&](int triggerMode) {
        SH101Envelope env;
        env.prepare(sr);
        env.setAttack(0.005);
        env.setDecay(0.5);
        env.setSustain(0.0);
        env.setRelease(0.1);
        env.setTriggerMode(triggerMode);
        env.noteOn(triggerMode == SH101Envelope::GateAndTrig);
        for (int i = 0; i < static_cast<int>(sr * 0.3); ++i) env.process();  // decayed away
        const double before = env.value();
        env.noteOn(triggerMode == SH101Envelope::GateAndTrig);
        for (int i = 0; i < static_cast<int>(sr * 0.01); ++i) env.process();
        return std::make_pair(before, env.value());
    };

    const auto gateTrig = runLegatoNoteChange(SH101Envelope::GateAndTrig);
    const auto gateOnly = runLegatoNoteChange(SH101Envelope::GateOnly);
    std::printf("      legato note change: GATE+TRIG %.3f -> %.3f | GATE %.3f -> %.3f\n",
                gateTrig.first, gateTrig.second, gateOnly.first, gateOnly.second);
    CHECK_LT(gateTrig.first, 0.1);               // decayed before the change
    CHECK_GT(gateTrig.second, 0.5);              // attack restarted
    CHECK_LT(gateOnly.second, 0.1);              // envelope just kept running

    // GATE mode must still start an attack when the envelope is not running.
    SH101Envelope fresh;
    fresh.prepare(sr);
    fresh.setAttack(0.05);
    fresh.setSustain(1.0);
    fresh.setTriggerMode(SH101Envelope::GateOnly);
    fresh.noteOn(false);
    for (int i = 0; i < static_cast<int>(sr * 0.1); ++i) fresh.process();
    CHECK_GT(fresh.value(), 0.5);

    // LFO mode: the envelope gate follows the modulator.
    SH101Envelope g3;
    g3.prepare(sr);
    g3.setAttack(0.005);
    g3.setDecay(0.05);
    g3.setSustain(0.0);
    g3.setRelease(0.02);
    g3.setTriggerMode(SH101Envelope::LfoTrigger);
    g3.noteOn(false);
    double peak = 0.0;
    bool sawIdle = false;
    for (int i = 0; i < static_cast<int>(sr * 1.0); ++i) {
        // 5 Hz modulator, gating the envelope high for half of each cycle.
        const double phase = std::fmod(static_cast<double>(i) * 5.0 / sr, 1.0);
        g3.setModGate(phase < 0.5);
        const double v = g3.process();
        peak = std::max(peak, v);
        if (v < 1e-6) sawIdle = true;
    }
    CHECK_GT(peak, 0.5);      // it does trigger
    CHECK(sawIdle);           // and it releases between modulator pulses

    // Without a key held, LFO mode must stay silent.
    g3.noteOff();
    for (int i = 0; i < static_cast<int>(sr * 0.5); ++i) {
        g3.setModGate(true);
        g3.process();
    }
    CHECK_LT(g3.value(), 1e-6);
}

// ---------------------------------------------------------------------------
// LFO

// Rate accuracy across the documented range.  The 0.1 Hz case needs a long
// window, so it is measured at a low sample rate (the LFO is rate-based, not
// sample-count-based).  The rate is derived from the interval between the first
// and last rising edge, not from edge-count/window, which would bias the result
// by up to one period.
SH101_TEST(lfo_rate_accuracy_across_documented_range) {
    for (double rate : { 0.1, 1.0, 5.0, 30.0 }) {
        const double sr = 2000.0;
        SH101LFO lfo;
        lfo.prepare(sr);
        lfo.setWave(SH101LFO::Square);
        lfo.setRate(rate);
        const int n = static_cast<int>(sr * (6.0 / std::min(rate, 5.0) + 1.5));
        int rises = 0;
        int firstEdge = -1;
        int lastEdge = -1;
        for (int i = 0; i < n; ++i) {
            lfo.process();
            if (lfo.squareRisingEdge()) {
                ++rises;
                if (firstEdge < 0) firstEdge = i;
                lastEdge = i;
            }
        }
        CHECK_GT(rises, 1.0);
        const double interval = static_cast<double>(lastEdge - firstEdge) / sr;
        const double measured = static_cast<double>(rises - 1) / interval;
        std::printf("      rate %.2f Hz -> measured %.4f Hz (%d edges over %.2f s)\n", rate,
                    measured, rises, interval);
        CHECK_NEAR(measured, rate, rate * 0.005);
    }
}

// Triangle and square shapes: the triangle is bipolar and symmetric, the square
// is high for half the cycle and its rising edges occur at the rate.
SH101_TEST(lfo_waveform_timing) {
    const double sr = 1000.0;
    SH101LFO lfo;
    lfo.prepare(sr);
    lfo.setRate(2.0);
    lfo.setWave(SH101LFO::Triangle);

    std::vector<double> tri;
    const int n = static_cast<int>(sr * 2.0);
    for (int i = 0; i < n; ++i) {
        lfo.process();
        tri.push_back(lfo.value());
    }
    CHECK_NEAR(peakAbs(tri), 1.0, 0.02);
    CHECK_NEAR(zeroCrossFrequency(tri, sr), 2.0, 0.05);

    lfo.setWave(SH101LFO::Square);
    lfo.reset();
    int high = 0;
    for (int i = 0; i < n; ++i) {
        lfo.process();
        if (lfo.squareHigh()) ++high;
    }
    const double highFraction = static_cast<double>(high) / n;
    CHECK_NEAR(highFraction, 0.5, 0.02);
}

// Random (stepped sample & hold) and noise (continuous wandering) must be
// distinct behaviours, as the brief requires.
SH101_TEST(lfo_random_is_stepped_and_noise_is_continuous) {
    const double sr = 1000.0;
    const double rate = 2.0;
    const int n = static_cast<int>(sr * 10.0);

    SH101LFO lfo;
    lfo.prepare(sr);
    lfo.setRate(rate);

    auto countChanges = [&](int wave) {
        lfo.setWave(wave);
        lfo.reset();
        int changes = 0;
        double previous = 0.0;
        bool first = true;
        double minV = 1e9, maxV = -1e9;
        for (int i = 0; i < n; ++i) {
            lfo.process();
            const double v = lfo.value();
            minV = std::min(minV, v);
            maxV = std::max(maxV, v);
            if (!first && v != previous) ++changes;
            previous = v;
            first = false;
        }
        return std::make_tuple(changes, minV, maxV);
    };

    const auto randomStats = countChanges(SH101LFO::Random);
    const auto noiseStats = countChanges(SH101LFO::Noise);
    const int randomChanges = std::get<0>(randomStats);
    const int noiseChanges = std::get<0>(noiseStats);

    std::printf("      random changes: %d (expected ~%d), noise changes: %d\n", randomChanges,
                static_cast<int>(rate * 10.0), noiseChanges);
    // Stepped: about one change per cycle.
    CHECK_LT(randomChanges, static_cast<int>(rate * 10.0) + 3);
    CHECK_GT(randomChanges, static_cast<int>(rate * 10.0) - 3);
    // Continuous: changes on a large fraction of samples.
    CHECK_GT(noiseChanges, n / 4);
    CHECK(std::get<1>(randomStats) < 1.0);
    CHECK(std::get<2>(randomStats) > -1.0);
    CHECK_LT(std::get<1>(noiseStats), 1.0);
}
