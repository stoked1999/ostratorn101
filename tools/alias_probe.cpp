// alias_probe — measures oscillator alias levels.
//
// Part of the calibration tooling, not the test suite: it prints a table of the
// strongest folded harmonic (the worst alias) relative to the fundamental for
// the band-limited model, side by side with a deliberately naive oscillator so
// the benefit of band-limiting is visible and any regression is obvious.
//
// Usage: alias_probe [sampleRate]
#include <cmath>
#include <cstdio>
#include <cstdlib>
#include <string>
#include <vector>

#include "sh101/Constants.h"
#include "sh101/SH101VCO.h"

using namespace sh101;

namespace {

// Naive (discontinuous) reference generator: saw and pulse without any
// band-limiting, used only as the comparison baseline in this tool.
struct NaiveOsc {
    double phase = 0.0;
    double saw(double dt) {
        const double v = 2.0 * phase - 1.0;
        phase += dt;
        if (phase >= 1.0) phase -= 1.0;
        return v;
    }
    double pulse(double dt, double pw) {
        const double v = (phase < pw) ? 1.0 : -1.0;
        phase += dt;
        if (phase >= 1.0) phase -= 1.0;
        return v;
    }
};

double goertzel(const std::vector<double>& x, double sr, double f) {
    const size_t n = x.size();
    const double w = kTwoPi * f / sr;
    const double coeff = 2.0 * std::cos(w);
    double s1 = 0.0, s2 = 0.0, winSum = 0.0;
    for (size_t i = 0; i < n; ++i) {
        const double win = 0.5 - 0.5 * std::cos(kTwoPi * static_cast<double>(i) / static_cast<double>(n - 1));
        winSum += win;
        const double s0 = x[i] * win + coeff * s1 - s2;
        s2 = s1;
        s1 = s0;
    }
    const double power = s1 * s1 + s2 * s2 - coeff * s1 * s2;
    return (winSum > 0.0) ? (2.0 * std::sqrt(std::max(0.0, power)) / winSum) : 0.0;
}

// Strongest alias: scan harmonic numbers whose direct frequency exceeds Nyquist
// and measure the folded component.  Returns dB relative to the fundamental.
struct AliasResult {
    double worstDb = -999.0;
    double worstFreq = 0.0;
    int worstHarmonic = 0;
};

AliasResult measureAlias(const std::vector<double>& x, double sr, double f0) {
    AliasResult r;
    const double fund = goertzel(x, sr, f0);
    if (fund <= 0.0) return r;
    for (int k = 2; k <= 200; ++k) {
        const double fk = k * f0;
        if (fk <= sr * 0.5) continue;
        if (fk > sr * 4.0) break;
        // Fold into the first Nyquist zone.
        double folded = std::fmod(fk, sr);
        if (folded > sr * 0.5) folded = sr - folded;
        if (folded < 20.0 || folded > sr * 0.5 - 20.0) continue;   // skip DC/Nyquist skirt
        // Skip probes that sit on a true harmonic (they would measure real signal).
        bool onHarmonic = false;
        for (int j = 1; j * f0 <= sr * 0.5; ++j) {
            if (std::fabs(folded - j * f0) < 30.0) { onHarmonic = true; break; }
        }
        if (onHarmonic) continue;
        const double amp = goertzel(x, sr, folded);
        const double levelDb = 20.0 * std::log10(std::max(1e-12, amp / fund));
        if (levelDb > r.worstDb) {
            r.worstDb = levelDb;
            r.worstFreq = folded;
            r.worstHarmonic = k;
        }
    }
    return r;
}

} // namespace

int main(int argc, char** argv) {
    const double sr = (argc > 1) ? std::atof(argv[1]) : 44100.0;
    const int n = 32768;
    std::printf("alias probe @ %.0f Hz, %d samples\n", sr, n);
    std::printf("%-8s %-6s | %-28s | %-28s\n", "f0", "pw", "model (polyBLEP)", "naive reference");
    std::printf("-------------------------------------------------------------------------------\n");

    for (double f0 : { 250.0, 500.0, 1000.0, 2000.0, 4000.0, 5000.0 }) {
        for (double pw : { 0.5, 0.25, 0.10 }) {
            // --- model ---
            SH101VCO vco;
            vco.prepare(sr);
            vco.setRange(1);
            vco.setPitchCV(frequencyToCV(f0));
            vco.setPulseWidth(pw);
            vco.reset();
            std::vector<double> saw, pulse;
            saw.reserve(n);
            pulse.reserve(n);
            for (int i = 0; i < n; ++i) {
                vco.process();
                saw.push_back(vco.saw());
                pulse.push_back(vco.pulse());
            }

            // --- naive reference ---
            NaiveOsc naive;
            std::vector<double> nSaw, nPulse;
            nSaw.reserve(n);
            nPulse.reserve(n);
            for (int i = 0; i < n; ++i) {
                const double dt = f0 / sr;
                nSaw.push_back(naive.saw(dt));
                nPulse.push_back(naive.pulse(dt, pw));
            }

            const AliasResult sawModel = measureAlias(pw == 0.5 ? saw : pulse, sr, f0);
            const AliasResult sawNaive = measureAlias(pw == 0.5 ? nSaw : nPulse, sr, f0);

            std::printf("%-8.1f %-6.2f | worst %7.1f dB @ %7.1f Hz (h%-3d) | worst %7.1f dB @ %7.1f Hz (h%-3d)\n",
                        f0, pw, sawModel.worstDb, sawModel.worstFreq, sawModel.worstHarmonic,
                        sawNaive.worstDb, sawNaive.worstFreq, sawNaive.worstHarmonic);
        }
    }
    return 0;
}
