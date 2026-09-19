// Oscillator validation: VCO frequency/scaling/level, sub oscillator division
// and phase locking, pulse width range, and alias behaviour.
//
// Validation list from the brief ("SONIC FIDELITY FIRST" / "Validation
// requirement"): oscillator frequency and scaling, waveform shape and
// pulse-width range, sub-oscillator phase/division behaviour.
//
// [APPROX] notes: the alias thresholds asserted here are for the polyBLEP
// implementation, which the brief accepts as a first implementation step; the
// numbers this test prints are the measured alias levels that a later
// CEM3340-derived model must beat.
#include <cstdio>
#include <vector>

#include "TestFramework.h"
#include "TestHelpers.h"
#include "sh101/SH101VCO.h"
#include "sh101/SubOscDivider.h"

using namespace sh101;
using namespace sh101test;

namespace {

// Render `n` samples of the oscillator and return the saw/pulse/core streams.
struct VcoCapture {
    std::vector<double> saw, pulse, core;
};

VcoCapture captureVco(SH101VCO& vco, int n) {
    VcoCapture c;
    c.saw.reserve(n);
    c.pulse.reserve(n);
    c.core.reserve(n);
    for (int i = 0; i < n; ++i) {
        vco.process();
        c.saw.push_back(vco.saw());
        c.pulse.push_back(vco.pulse());
        c.core.push_back(vco.coreSquare());
    }
    return c;
}

} // namespace

// ---------------------------------------------------------------------------
// Frequency accuracy over the MIDI range at the 8' range setting.
// Tolerance: 6 cents (0.35%) — polyBLEP has a small frequency-independent bias
// only through the duty/phase handling; the documented calibration target is
// exact 1 V/oct tracking.
SH101_TEST(vco_frequency_accuracy_over_midi_range) {
    const double sr = 48000.0;
    SH101VCO vco;
    vco.prepare(sr);
    vco.setRange(1);       // 8'
    vco.setPulseWidth(0.5);

    for (int note : { 24, 36, 48, 60, 69, 72, 84, 96, 108 }) {
        const double expected = cvToFrequency(midiNoteToCV(note));
        vco.setPitchCV(midiNoteToCV(note));
        vco.reset();
        const int n = static_cast<int>(sr * 0.5);
        VcoCapture c = captureVco(vco, n);
        const double measured = zeroCrossFrequency(c.saw, sr);
        CHECK_NEAR(measured, expected, expected * 0.0035);
        if (note == 60 || note == 108) {
            std::printf("      note %3d: expected %9.4f Hz, measured %9.4f Hz\n", note, expected,
                        measured);
        }
    }
}

// Octave switching must exactly halve/double the frequency (service note:
// "range width" is a separate trim, so the ratio must be exact in the nominal
// calibration).
SH101_TEST(vco_octave_switching_doubles_or_halves) {
    const double sr = 48000.0;
    SH101VCO vco;
    vco.prepare(sr);
    vco.setPitchCV(midiNoteToCV(60));

    double f[4];
    for (int r = 0; r < 4; ++r) {
        vco.setRange(r);
        vco.reset();
        VcoCapture c = captureVco(vco, static_cast<int>(sr * 0.5));
        f[r] = zeroCrossFrequency(c.saw, sr);
    }
    // 8' is the reference; 16' is one octave down, 4' one up, 2' two up.
    CHECK_NEAR(f[0], f[1] / 2.0, f[1] * 0.002);
    CHECK_NEAR(f[2], f[1] * 2.0, f[1] * 0.004);
    CHECK_NEAR(f[3], f[1] * 4.0, f[1] * 0.008);
    std::printf("      ranges 16/8/4/2 = %.3f / %.3f / %.3f / %.3f Hz\n", f[0], f[1], f[2], f[3]);
}

// The front-panel tune control spans roughly +/-50 cents; the service "VCO
// tune" trim is a static offset in the same domain.
SH101_TEST(vco_fine_tune_range_is_about_50_cents) {
    const double sr = 48000.0;
    SH101VCO vco;
    vco.prepare(sr);
    vco.setRange(1);
    vco.setPitchCV(midiNoteToCV(60));

    vco.setFineTuneCents(0.0);
    vco.reset();
    const double base = zeroCrossFrequency(captureVco(vco, static_cast<int>(sr * 0.5)).saw, sr);

    vco.setFineTuneCents(50.0);
    vco.reset();
    const double up = zeroCrossFrequency(captureVco(vco, static_cast<int>(sr * 0.5)).saw, sr);

    vco.setFineTuneCents(-50.0);
    vco.reset();
    const double down = zeroCrossFrequency(captureVco(vco, static_cast<int>(sr * 0.5)).saw, sr);

    CHECK_NEAR(up / base, std::pow(2.0, 50.0 / 1200.0), 0.002);
    CHECK_NEAR(down / base, std::pow(2.0, -50.0 / 1200.0), 0.002);
}

// Pulse width: measured duty must follow the requested width across the safe
// non-zero band, and must never collapse to zero or one.
SH101_TEST(vco_pulse_width_range_and_safety) {
    const double sr = 48000.0;
    SH101VCO vco;
    vco.prepare(sr);
    vco.setRange(1);
    vco.setPitchCV(midiNoteToCV(60));   // ~261 Hz

    for (double w : { 0.03, 0.1, 0.25, 0.5 }) {
        vco.setPulseWidth(w);
        vco.reset();
        const int n = static_cast<int>(sr * 0.5);
        VcoCapture c = captureVco(vco, n);
        const double duty = dutyCycle(c.pulse);
        CHECK_NEAR(duty, w, 0.01);
        // Peak-to-peak of a pulse of any width stays within the CEM3340's
        // output scaling; nothing is allowed to blow up at extreme widths.
        CHECK_LT(peakAbs(c.pulse), 1.5);
    }

    // Requests outside the safe band are clamped, not honoured literally.
    vco.setPulseWidth(0.0);
    vco.reset();
    const double dutyMin = dutyCycle(captureVco(vco, static_cast<int>(sr * 0.25)).pulse);
    CHECK_GT(dutyMin, 0.02);
    vco.setPulseWidth(1.0);
    vco.reset();
    const double dutyMax = dutyCycle(captureVco(vco, static_cast<int>(sr * 0.25)).pulse);
    CHECK_LT(dutyMax, 0.98);
}

// Aliasing: the brief forbids naive discontinuous waveforms and accepts
// polyBLEP/minBLEP as a first implementation step.
//
// Measured baseline (see build/alias_probe.exe, tools/alias_probe.cpp, which
// scans every folded harmonic, not one probe bin):
//     f0      pw    model        naive reference
//     500    0.50   -41.3 dB     +15.8 dB
//     500    0.10   -31.1 dB     +31.2 dB
//     1000   0.10   -27.3 dB     +35.7 dB
//     5000   0.50   -24.0 dB     +33.4 dB
// So the band-limited model is 40..65 dB better than a naive oscillator, and its
// worst alias at high pitch is about -24 dB.  Those are the numbers a
// CEM3340-derived model has to improve on, and they are what this test pins.
SH101_TEST(vco_alias_level_is_bounded) {
    const double sr = 44100.0;
    const int n = 32768;

    struct Case { double f0; double pw; double limitDb; const char* label; };
    const Case cases[] = {
        { 500.0, 0.50, -35.0, "saw 500 Hz" },
        { 500.0, 0.10, -25.0, "narrow pulse 500 Hz / 10% duty" },
        { 5000.0, 0.50, -20.0, "saw 5 kHz (known polyBLEP limit)" },
        { 5000.0, 0.10, -10.0, "narrow pulse 5 kHz / 10% duty (0.9 samples high)" },
    };

    for (const Case& c : cases) {
        SH101VCO vco;
        vco.prepare(sr);
        vco.setRange(1);
        vco.setPitchCV(frequencyToCV(c.f0));
        vco.setPulseWidth(c.pw);
        vco.reset();

        std::vector<double> out;
        out.reserve(n);
        for (int i = 0; i < n; ++i) {
            vco.process();
            out.push_back(c.pw == 0.5 ? vco.saw() : vco.pulse());
        }

        const double fund = goertzelAmplitude(out, sr, c.f0);
        CHECK_GT(fund, 0.0);
        double worst = -999.0;
        for (int k = 2; k <= 200; ++k) {
            const double fk = k * c.f0;
            if (fk <= sr * 0.5) continue;
            if (fk > sr * 4.0) break;
            double folded = std::fmod(fk, sr);
            if (folded > sr * 0.5) folded = sr - folded;
            if (folded < 20.0 || folded > sr * 0.5 - 20.0) continue;
            bool onHarmonic = false;
            for (int j = 1; j * c.f0 <= sr * 0.5; ++j) {
                if (std::fabs(folded - j * c.f0) < 30.0) { onHarmonic = true; break; }
            }
            if (onHarmonic) continue;
            worst = std::max(worst, db(goertzelAmplitude(out, sr, folded) / fund));
        }
        std::printf("      %-46s worst alias %7.1f dB (limit %.1f)\n", c.label, worst, c.limitDb);
        CHECK_LT(worst, c.limitDb);
        CHECK(allFinite(out));
    }
}

// ---------------------------------------------------------------------------
// Sub oscillator: division, phase locking to the VCO edge stream, and the
// narrow-pulse mode's duty cycle.
SH101_TEST(sub_osc_frequency_is_exact_division_of_vco) {
    const double sr = 48000.0;
    SH101VCO vco;
    SH101VCO subSource;
    SubOscDivider sub;
    vco.prepare(sr);
    subSource.prepare(sr);
    sub.prepare(sr);

    vco.setRange(1);
    vco.setPitchCV(midiNoteToCV(60));
    subSource.setRange(1);
    subSource.setPitchCV(midiNoteToCV(60));

    const int n = static_cast<int>(sr * 1.0);
    for (int mode = 0; mode < 3; ++mode) {
        sub.setMode(mode);
        sub.reset();
        vco.reset();
        std::vector<double> subOut;
        subOut.reserve(n);
        for (int i = 0; i < n; ++i) {
            vco.process();
            sub.process(vco.phase(), vco.wrapped(), vco.frequency() / sr);
            subOut.push_back(sub.value());
        }
        const double fSub = zeroCrossFrequency(subOut, sr);
        const double fExpected = vco.frequency() / (mode == 0 ? 2.0 : 4.0);
        CHECK_NEAR(fSub, fExpected, fExpected * 0.005);

        if (mode == 2) {
            const double duty = dutyCycle(subOut);
            CHECK_NEAR(duty, 0.25, 0.02);
        } else {
            const double duty = dutyCycle(subOut);
            CHECK_NEAR(duty, 0.5, 0.02);
        }
    }
}

// Every sub oscillator transition must land on a VCO cycle boundary: this is
// the "do not make the sub oscillator an independent oscillator" requirement.
SH101_TEST(sub_osc_edges_are_phase_locked_to_vco) {
    const double sr = 48000.0;
    SH101VCO vco;
    SubOscDivider sub;
    vco.prepare(sr);
    sub.prepare(sr);
    vco.setRange(1);
    vco.setPitchCV(midiNoteToCV(48));   // 130.8 Hz, so ~2s of audio is plenty

    for (int mode = 0; mode < 3; ++mode) {
        sub.setMode(mode);
        sub.reset();
        vco.reset();

        int lastSubSign = 0;
        int lastWrapIndex = -1000;
        int transitions = 0;
        int aligned = 0;
        const int n = static_cast<int>(sr * 0.5);
        for (int i = 0; i < n; ++i) {
            vco.process();
            if (vco.wrapped()) lastWrapIndex = i;
            sub.process(vco.phase(), vco.wrapped(), vco.frequency() / sr);
            const int sign = (sub.value() > 0.0) ? 1 : -1;
            if (lastSubSign != 0 && sign != lastSubSign) {
                ++transitions;
                // A transition may be reported on the wrap sample itself or on
                // the following one (the polyBLEP correction straddles it).
                if (i - lastWrapIndex <= 1) ++aligned;
            }
            lastSubSign = sign;
        }
        CHECK_GT(transitions, 10.0);
        CHECK_NEAR(static_cast<double>(aligned), static_cast<double>(transitions), 0.0);
    }
}

// The sub oscillator must restart from a defined state on gate-on, exactly like
// the hardware flip-flops being cleared.
SH101_TEST(sub_osc_reset_is_deterministic) {
    const double sr = 48000.0;
    SH101VCO vco;
    SubOscDivider sub;
    vco.prepare(sr);
    sub.prepare(sr);
    sub.setMode(1);
    vco.setRange(1);
    vco.setPitchCV(midiNoteToCV(60));

    std::vector<double> first, second;
    for (int pass = 0; pass < 2; ++pass) {
        sub.reset();
        vco.reset();
        std::vector<double> out;
        for (int i = 0; i < 4096; ++i) {
            vco.process();
            sub.process(vco.phase(), vco.wrapped(), vco.frequency() / sr);
            out.push_back(sub.value());
        }
        if (pass == 0) first = out; else second = out;
    }
    bool identical = (first.size() == second.size());
    for (size_t i = 0; identical && i < first.size(); ++i) {
        identical = (first[i] == second[i]);
    }
    CHECK(identical);
}
