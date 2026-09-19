// Analog-variation validation.
//
// The brief's fidelity section asks for realistic analog variation and warns, in
// the same paragraph, against "exaggerated random drift or instability that makes
// the instrument sound less like a well-calibrated SH-101".  So the tests here
// check both halves of that sentence: that the variation is *present* (a perfectly
// static oscillator is not what an analog instrument sounds like) and that it is
// *bounded* by the tolerance budget in Calibration.h — a healthy unit, not a
// broken one.
//
// Every figure checked here is a calibration target for fidelity milestone 3
// (fit against a measured unit); the tolerances in this file are acceptance
// thresholds for the model, not measurements of hardware.
#include <cmath>
#include <cstdio>
#include <vector>

#include "TestFramework.h"
#include "TestHelpers.h"
#include "sh101/SH101Engine.h"

using namespace sh101;
using namespace sh101test;

namespace {

constexpr double kSampleRate = 48000.0;

SH101Params analogTestParams() {
    SH101Params p;
    p.sawLevel = 1.0;
    p.pulseLevel = 0.0;
    p.subLevel = 0.3;
    p.cutoff = 2000.0;
    p.resonance = 0.2;
    p.attack = 0.002;
    p.decay = 0.3;
    p.sustain = 0.8;
    p.release = 0.2;
    p.volume = 0.8;
    return p;
}

double rmsOf(const std::vector<double>& x) {
    if (x.empty()) return 0.0;
    double s = 0.0;
    for (double v : x) s += v * v;
    return std::sqrt(s / static_cast<double>(x.size()));
}

} // namespace

// The slow drift must move (an instrument that never moves is a plugin, not an
// analog synth) but must stay inside the VCO tolerance budget.
SH101_TEST(analog_pitch_drift_moves_but_stays_in_budget) {
    SH101Engine engine;
    engine.prepare(kSampleRate, 2);
    engine.setDeterministicTestMode(true, 0x51a7u);
    engine.setParams(analogTestParams());
    engine.noteOn(48, 1.0);

    const double budget = engine.calibration().driftPitchCents
                          + engine.calibration().noteToleranceCents;

    std::vector<double> offsets;
    for (int block = 0; block < 600; ++block) {          // 10 s at 1 ms blocks
        for (int i = 0; i < 48; ++i) engine.renderSample();
        offsets.push_back(engine.analogVariation().pitchDriftCents());
    }

    double peak = 0.0;
    double mean = 0.0;
    for (double v : offsets) {
        peak = std::max(peak, std::fabs(v));
        mean += v;
    }
    mean /= static_cast<double>(offsets.size());
    const double spread = [&] {
        double s = 0.0;
        for (double v : offsets) s += (v - mean) * (v - mean);
        return std::sqrt(s / static_cast<double>(offsets.size()));
    }();

    std::printf("      drift: peak %.3f cents, mean %.3f cents, spread %.3f cents "
                "(budget %.2f cents)\n", peak, mean, spread, budget);

    CHECK_GT(peak, 0.05);                       // it does move
    CHECK_GT(spread, 0.05);                     // and it is not a constant offset
    CHECK_LT(peak, budget);                     // but never leaves the budget
    CHECK_NEAR(mean, 0.0, 0.5);                 // and is centred on the tuned pitch
}

// The per-note tolerance must differ between notes, stay inside its own budget,
// and be reproducible for a given seed.
SH101_TEST(analog_note_tolerance_is_per_note_and_reproducible) {
    const Calibration cal{};
    auto collect = [](uint32_t seed) {
        SH101Engine engine;
        engine.prepare(kSampleRate, 2);
        engine.setDeterministicTestMode(true, seed);
        engine.setParams(analogTestParams());
        std::vector<double> perNote;
        for (int note = 0; note < 12; ++note) {
            engine.noteOn(40 + note, 1.0);
            engine.renderSample();
            perNote.push_back(engine.analogVariation().noteToleranceAppliedCents());
            engine.noteOff(40 + note);
            for (int i = 0; i < 64; ++i) engine.renderSample();
        }
        return perNote;
    };

    const std::vector<double> first = collect(0x1234u);
    const std::vector<double> again = collect(0x1234u);

    double minValue = 1.0e9;
    double maxValue = -1.0e9;
    for (size_t i = 0; i < first.size(); ++i) {
        minValue = std::min(minValue, first[i]);
        maxValue = std::max(maxValue, first[i]);
        CHECK_NEAR(first[i], again[i], 1.0e-12);        // same seed, same notes
        CHECK_LT(std::fabs(first[i]), cal.noteToleranceCents + 1.0e-9);
    }
    std::printf("      per-note tolerance over 12 notes: %.3f .. %.3f cents (budget +/- %.2f)\n",
                minValue, maxValue, cal.noteToleranceCents);
    CHECK_GT(maxValue - minValue, 0.05);               // notes are not all identical
}

SH101_TEST(analog_cutoff_drift_stays_in_budget) {
    SH101Engine engine;
    engine.prepare(kSampleRate, 2);
    engine.setDeterministicTestMode(true, 0x77u);
    engine.setParams(analogTestParams());
    engine.noteOn(48, 1.0);

    const double budget = engine.calibration().driftCutoffOctaves;
    double peak = 0.0;
    double spreadBefore = 0.0;
    double spreadAfter = 0.0;
    for (int block = 0; block < 400; ++block) {
        for (int i = 0; i < 64; ++i) engine.renderSample();
        const double v = engine.analogVariation().cutoffDriftOctaves();
        peak = std::max(peak, std::fabs(v));
        spreadAfter = v;
    }
    std::printf("      cutoff drift: peak %.5f octaves (budget %.5f), ~%.2f cents\n",
                peak, budget, std::fabs(spreadAfter - spreadBefore) * 1200.0);
    CHECK_GT(peak, 1.0e-5);
    CHECK_LT(peak, budget * 1.001);
}

// The envelope's per-note RC tolerance must be small enough that the documented
// envelope times stay the documented envelope times.
SH101_TEST(analog_envelope_time_tolerance_is_bounded) {
    SH101Engine engine;
    engine.prepare(kSampleRate, 2);
    engine.setDeterministicTestMode(true, 0x99u);
    engine.setParams(analogTestParams());

    const double budget = engine.calibration().envTimeTolerance;
    double minScale = 1.0e9;
    double maxScale = -1.0e9;
    for (int note = 0; note < 16; ++note) {
        engine.noteOn(45 + note, 1.0);
        engine.renderSample();
        const double scale = engine.analogVariation().envelopeTimeScale();
        minScale = std::min(minScale, scale);
        maxScale = std::max(maxScale, scale);
        engine.noteOff(45 + note);
        for (int i = 0; i < 32; ++i) engine.renderSample();
    }
    std::printf("      envelope time scale over 16 notes: %.4f .. %.4f (budget 1 +/- %.3f)\n",
                minScale, maxScale, budget);
    CHECK_LT(std::fabs(minScale - 1.0), budget + 1.0e-9);
    CHECK_LT(std::fabs(maxScale - 1.0), budget + 1.0e-9);
    CHECK_GT(maxScale - minScale, 1.0e-4);      // notes really do differ
}

// The output stage hiss: present (an analog output amplifier is never digital
// silent) at the calibrated level, and low enough that the brief's silence
// requirement is still met.
SH101_TEST(analog_noise_floor_is_present_and_at_the_calibrated_level) {
    SH101Engine engine;
    engine.prepare(kSampleRate, 2);
    engine.setDeterministicTestMode(true, 0xabcdu);
    engine.setParams(analogTestParams());

    std::vector<double> silence;
    for (int i = 0; i < 96000; ++i) silence.push_back(engine.renderSample());

    const double rms = rmsOf(silence);
    const double dbfs = 20.0 * std::log10(std::max(rms, 1.0e-12));
    const double expected = engine.calibration().noiseFloorDbfs;
    const double peak = [&] {
        double m = 0.0;
        for (double v : silence) m = std::max(m, std::fabs(v));
        return m;
    }();

    std::printf("      idle hiss: %.1f dBFS RMS (calibrated %.1f), peak %.6f\n", dbfs, expected, peak);
    CHECK_NEAR(dbfs, expected, 1.5);            // the calibrated level, within 1.5 dB
    CHECK_LT(peak, 1.0e-3);                     // still "silent" by the brief's test
}

// With the variation switched off, the model is the calibrated core again: no
// hiss, no drift, exactly ideal CV.  This is what the measurement tests rely on.
SH101_TEST(analog_variation_off_is_exactly_ideal) {
    SH101Engine engine;
    engine.prepare(kSampleRate, 2);
    engine.setDeterministicTestMode(true, 0x1111u);
    engine.setAnalogVariationEnabled(false);
    engine.setParams(analogTestParams());
    engine.noteOn(60, 1.0);
    for (int i = 0; i < 256; ++i) engine.renderSample();

    CHECK_NEAR(engine.analogVariation().pitchOffsetCents(), 0.0, 1.0e-12);
    CHECK_NEAR(engine.analogVariation().cutoffOffsetOctaves(), 0.0, 1.0e-15);
    CHECK_NEAR(engine.analogVariation().envelopeTimeScale(), 1.0, 1.0e-12);
    CHECK_NEAR(engine.analogVariation().noiseFloor(), 0.0, 1.0e-15);
    // And the rendered silence is exactly zero, not merely small.
    CHECK_NEAR(engine.analogVariation().noiseFloorRms(), 0.0, 1.0e-15);
}

// Drift must not change the *level structure* of the instrument: it is a
// micro-variation, not a different sound.
SH101_TEST(analog_variation_does_not_change_the_level_structure) {
    auto renderPeak = [](bool variationOn) {
        SH101Engine engine;
        engine.prepare(kSampleRate, 2);
        engine.setDeterministicTestMode(true, 0x2222u);
        engine.setAnalogVariationEnabled(variationOn);
        engine.setParams(analogTestParams());
        engine.noteOn(48, 1.0);
        std::vector<double> audio;
        for (int i = 0; i < 48000; ++i) audio.push_back(engine.renderSample());
        return peakAbs(audio);
    };

    const double withVariation = renderPeak(true);
    const double withoutVariation = renderPeak(false);
    const double difference = std::fabs(withVariation - withoutVariation) / withoutVariation;
    std::printf("      peak: %.5f with variation, %.5f ideal (%.2f%% difference)\n",
                withVariation, withoutVariation, difference * 100.0);
    CHECK_LT(difference, 0.10);                 // within 10%: the same instrument
}

// The variation must not break the two determinism guarantees the suite already
// makes: reproducible renders for a seed, and buffer-size independence (which is
// why the drift advances per sample, not per block).
SH101_TEST(analog_variation_keeps_renders_deterministic) {
    auto render = [](uint32_t seed, int blockSize) {
        SH101Engine engine;
        engine.prepare(kSampleRate, 2);
        engine.setDeterministicTestMode(true, seed);
        engine.setParams(analogTestParams());
        engine.noteOn(50, 1.0);

        std::vector<double> audio;
        std::vector<float> block(static_cast<size_t>(blockSize));
        const int total = 24000;
        int done = 0;
        while (done < total) {
            const int n = std::min(blockSize, total - done);
            engine.renderBlock(block.data(), n);
            for (int i = 0; i < n; ++i) audio.push_back(block[static_cast<size_t>(i)]);
            done += n;
        }
        return audio;
    };

    const std::vector<double> a = render(0x3333u, 512);
    const std::vector<double> b = render(0x3333u, 512);
    const std::vector<double> singleSample = render(0x3333u, 1);

    double maxDifference = 0.0;
    for (size_t i = 0; i < a.size(); ++i) {
        maxDifference = std::max(maxDifference, std::fabs(a[i] - b[i]));
    }
    CHECK_NEAR(maxDifference, 0.0, 1.0e-15);                // same seed, same render

    double blockDifference = 0.0;
    for (size_t i = 0; i < a.size(); ++i) {
        blockDifference = std::max(blockDifference, std::fabs(a[i] - singleSample[i]));
    }
    std::printf("      same-seed difference %.3g, block-size difference %.3g\n", maxDifference,
                blockDifference);
    CHECK_NEAR(blockDifference, 0.0, 1.0e-15);              // buffer size does not matter
}
