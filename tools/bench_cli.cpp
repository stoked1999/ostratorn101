// bench_cli — measures the model's CPU cost against the brief's performance
// targets ("keep one instance comfortably real-time on a normal modern CPU").
//
// Reports the real-time factor for 1x/2x/4x oversampling at the supported host
// sample rates, both for a plain patch and for the worst case (maximum
// resonance, all four sources, deep modulation).
#include <chrono>
#include <cstdio>
#include <vector>

#include "sh101/SH101Engine.h"

using namespace sh101;

namespace {

struct BenchResult {
    double realTimeFactor = 0.0;   // seconds of audio per second of CPU
    double percentOfOneCore = 0.0;
};

BenchResult bench(double sr, int oversample, const SH101Params& p, double seconds) {
    SH101Engine engine;
    engine.prepare(sr, oversample);
    engine.setParams(p);
    engine.noteOn(48, 1.0);
    engine.noteOn(60, 1.0);

    const int total = static_cast<int>(sr * seconds);
    std::vector<float> block(512);
    const auto t0 = std::chrono::steady_clock::now();
    int done = 0;
    while (done < total) {
        const int n = std::min<int>(512, total - done);
        engine.renderBlock(block.data(), n);
        done += n;
    }
    const auto t1 = std::chrono::steady_clock::now();
    const double wall = std::chrono::duration<double>(t1 - t0).count();

    BenchResult r;
    r.realTimeFactor = seconds / wall;
    r.percentOfOneCore = 100.0 / r.realTimeFactor;
    return r;
}

} // namespace

int main(int argc, char** argv) {
    const double seconds = (argc > 1) ? std::atof(argv[1]) : 10.0;

    SH101Params plain;
    plain.sawLevel = 0.8;
    plain.pulseLevel = 0.5;
    plain.subLevel = 0.5;
    plain.cutoff = 2000.0;
    plain.resonance = 0.3;
    plain.filterEnvAmt = 2.0;
    plain.attack = 0.005;
    plain.decay = 0.5;
    plain.sustain = 0.6;
    plain.release = 0.3;
    plain.pwmSource = 2;
    plain.pwmAmount = 0.4;
    plain.lfoRate = 5.0;

    SH101Params worst = plain;
    worst.resonance = 1.0;
    worst.noiseLevel = 1.0;
    worst.sawLevel = 1.0;
    worst.pulseLevel = 1.0;
    worst.subLevel = 1.0;
    worst.pwmAmount = 1.0;
    worst.vcoModDep = 1.0;
    worst.filterModAmt = 6.0;
    worst.filterEnvAmt = 6.0;
    worst.keyTrack = 1.0;
    worst.lfoRate = 30.0;
    worst.lfoWave = SH101LFO::Noise;
    worst.attack = 0.0015;
    worst.decay = 0.002;

    std::printf("SH-101 model CPU benchmark: %.1f s of audio per measurement\n", seconds);
    std::printf("%-8s %-4s | %-22s | %-22s\n", "rate", "os", "plain patch", "worst case");
    for (double sr : { 44100.0, 48000.0, 88200.0, 96000.0 }) {
        for (int os : { 1, 2, 4 }) {
            const BenchResult a = bench(sr, os, plain, seconds);
            const BenchResult b = bench(sr, os, worst, seconds);
            std::printf("%-8.0f %-4d | %6.1fx realtime %4.1f%% | %6.1fx realtime %4.1f%%\n", sr, os,
                        a.realTimeFactor, a.percentOfOneCore, b.realTimeFactor, b.percentOfOneCore);
        }
    }
    return 0;
}
