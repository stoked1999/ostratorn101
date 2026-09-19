// Source mixer and VCA validation.
//
// Validation list from the brief: "mixer gain structure", "VCA response".
#include <cstdio>
#include <vector>

#include "TestFramework.h"
#include "TestHelpers.h"
#include "sh101/SH101VCA.h"
#include "sh101/SourceMixer.h"

using namespace sh101;
using namespace sh101test;

// Per-source gain staging: the mixer's linear sum must equal the calibrated
// gains, so a level change is exactly a gain change (the nonlinear part is only
// the input stage's soft clipper).
SH101_TEST(mixer_gain_structure_matches_calibration) {
    Calibration cal;
    SourceMixer mixer;
    mixer.prepare(48000.0, cal);

    mixer.setLevels(1.0, 0.0, 0.0, 0.0);
    mixer.process(1.0, 1.0, 1.0, 1.0);
    CHECK_NEAR(mixer.lastSum(), cal.sawMixGain, 1e-12);

    mixer.setLevels(0.0, 1.0, 0.0, 0.0);
    mixer.process(1.0, 1.0, 1.0, 1.0);
    CHECK_NEAR(mixer.lastSum(), cal.pulseMixGain, 1e-12);

    mixer.setLevels(0.0, 0.0, 1.0, 0.0);
    mixer.process(1.0, 1.0, 1.0, 1.0);
    CHECK_NEAR(mixer.lastSum(), cal.subMixGain, 1e-12);

    mixer.setLevels(0.0, 0.0, 0.0, 1.0);
    mixer.process(1.0, 1.0, 1.0, 1.0);
    CHECK_NEAR(mixer.lastSum(), cal.noiseMixGain, 1e-12);

    // Summing all four is linear before the clipper...
    mixer.setLevels(1.0, 1.0, 1.0, 1.0);
    mixer.process(1.0, 1.0, 1.0, 1.0);
    const double expected = cal.sawMixGain + cal.pulseMixGain + cal.subMixGain + cal.noiseMixGain;
    CHECK_NEAR(mixer.lastSum(), expected, 1e-12);

    // ...and half-scale levels give exactly half the sum.
    mixer.setLevels(0.5, 0.5, 0.5, 0.5);
    mixer.process(1.0, 1.0, 1.0, 1.0);
    CHECK_NEAR(mixer.lastSum(), expected * 0.5, 1e-12);
}

// The level into the IR3109 changes its nonlinear behaviour, so the mixer drives
// an explicit soft-clip stage rather than summing at unity.
SH101_TEST(mixer_drive_stage_compresses_and_bounds) {
    Calibration cal;
    SourceMixer mixer;
    mixer.prepare(48000.0, cal);
    mixer.setLevels(1.0, 1.0, 1.0, 1.0);

    mixer.process(1.0, 1.0, 1.0, 1.0);
    const double driven = mixer.lastOutput();
    const double linear = mixer.lastSum();
    CHECK_LT(driven, linear);                    // compression
    CHECK_LT(driven, cal.mixerHeadroom * 1.01);  // bounded by the headroom

    // Small signals stay essentially linear (the clipper only bends near the
    // rail, which is what keeps clean patches clean).
    mixer.process(0.1, 0.1, 0.1, 0.1);
    CHECK_NEAR(mixer.lastOutput(), mixer.lastSum(), std::fabs(mixer.lastSum()) * 0.05);

    mixer.setLevels(0.0, 0.0, 0.0, 0.0);
    mixer.process(1.0, 1.0, 1.0, 1.0);
    CHECK_NEAR(mixer.lastOutput(), 0.0, 1e-12);
}

// VCA control law and modes (BA662A): ENV follows the envelope, GATE is a fixed
// level while the gate is high, and a closed VCA only leaks.
SH101_TEST(vca_env_and_gate_modes) {
    Calibration cal;
    SH101VCA vca;
    vca.prepare(48000.0, cal);

    vca.setMode(SH101VCA::Env);
    const double x = 0.1;
    const double hr = 1.0 / cal.vcaNonlinearity;              // nl = 0.2 -> hr = 5
    const double shaped = hr * std::tanh(x / hr);
    for (double env : { 0.0, 0.25, 0.5, 0.75, 1.0 }) {
        const double y = vca.process(x, env);
        // Envelope gain plus the closed-VCA feedthrough term.
        const double expected = env * cal.vcaGainScale * shaped + cal.vcaOffLeakage * x;
        CHECK_NEAR(y, expected, 1e-12);
        CHECK_NEAR(vca.lastGain(), env * cal.vcaGainScale, 1e-12);
    }

    vca.setMode(SH101VCA::Gate);
    vca.setGate(true);
    vca.process(x, 0.0);
    CHECK_NEAR(vca.lastGain(), cal.vcaGainScale, 1e-12);
    CHECK_NEAR(vca.process(x, 0.0), cal.vcaGainScale * shaped + cal.vcaOffLeakage * x, 1e-12);
    vca.setGate(false);
    const double closed = vca.process(x, 1.0);
    CHECK_LT(std::fabs(closed), 1e-3);      // ~-80 dB feedthrough
    CHECK_NEAR(vca.lastGain(), 0.0, 1e-12);
}

// The OTA's differential pair compresses large signals, which is part of the
// BA662 path's character; small signals stay linear.
SH101_TEST(vca_nonlinearity_shape) {
    Calibration cal;
    SH101VCA vca;
    vca.prepare(48000.0, cal);
    vca.setMode(SH101VCA::Env);

    const double small = vca.process(0.01, 1.0);
    CHECK_NEAR(small, 0.01 + cal.vcaOffLeakage * 0.01, 0.0002);   // linear at small signal

    const double large = vca.process(3.0, 1.0);
    const double idealLarge = 3.0 + cal.vcaOffLeakage * 3.0;
    CHECK_LT(large, idealLarge);                 // compressed at large signal
    CHECK_GT(large, 1.5 * cal.vcaGainScale);     // but not crushed

    // An ideal (nl = 0) VCA is perfectly linear, which proves the compression
    // comes from the modelled OTA and not from a stray clipper.
    Calibration ideal = cal;
    ideal.vcaNonlinearity = 0.0;
    SH101VCA idealVca;
    idealVca.prepare(48000.0, ideal);
    idealVca.setMode(SH101VCA::Env);
    CHECK_NEAR(idealVca.process(3.0, 1.0), 3.0 + ideal.vcaOffLeakage * 3.0, 1e-5);
}
