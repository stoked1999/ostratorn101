// sh101_render — headless renderer for the SH-101 model.
//
// This is the "headless DSP test executable" companion: it renders the model to
// a WAV file so the sound can be auditioned without any plugin build, and so
// calibration measurements can be made on deterministic renders.
//
// Usage:
//   sh101_render [--patch N] [--seconds S] [--sr HZ] [--os 1|2|4] out.wav
//   sh101_render --list
//
// Patches are named after the behaviours the brief calls out as sonic
// priorities, and are intentionally plain: each sets only documented controls.
#include <array>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <string>
#include <vector>

#include "sh101/Presets.h"
#include "sh101/SH101Engine.h"

using namespace sh101;

namespace {

// ---- Minimal 16-bit PCM mono WAV writer -----------------------------------
struct WavWriter {
    static bool write(const std::string& path, const std::vector<float>& samples, int sampleRate) {
        FILE* f = std::fopen(path.c_str(), "wb");
        if (!f) return false;
        const uint32_t dataBytes = static_cast<uint32_t>(samples.size() * 2);
        const uint32_t chunkSize = 36 + dataBytes;
        const uint16_t channels = 1;
        const uint16_t bits = 16;
        const uint32_t byteRate = static_cast<uint32_t>(sampleRate) * channels * bits / 8;
        const uint16_t blockAlign = static_cast<uint16_t>(channels * bits / 8);
        std::fwrite("RIFF", 1, 4, f);
        std::fwrite(&chunkSize, 4, 1, f);
        std::fwrite("WAVE", 1, 4, f);
        std::fwrite("fmt ", 1, 4, f);
        const uint32_t fmtSize = 16;
        const uint16_t audioFmt = 1;
        std::fwrite(&fmtSize, 4, 1, f);
        std::fwrite(&audioFmt, 2, 1, f);
        std::fwrite(&channels, 2, 1, f);
        std::fwrite(&sampleRate, 4, 1, f);
        std::fwrite(&byteRate, 4, 1, f);
        std::fwrite(&blockAlign, 2, 1, f);
        std::fwrite(&bits, 2, 1, f);
        std::fwrite("data", 1, 4, f);
        std::fwrite(&dataBytes, 4, 1, f);
        for (float s : samples) {
            const double clipped = clampd(static_cast<double>(s), -1.0, 1.0);
            const int16_t v = static_cast<int16_t>(clipped * 32767.0);
            std::fwrite(&v, 2, 1, f);
        }
        std::fclose(f);
        return true;
    }
};

struct PatchInfo {
    const char* name;
    const char* description;
};

const PatchInfo kPatches[] = {
    { "init",      "Saw, filter open, short envelope (reference patch)" },
    { "bass",      "Saw+sub, punchy filter envelope, medium resonance" },
    { "pluck",     "Saw, fast attack, no sustain, deep filter envelope" },
    { "selfosc",   "Maximum resonance self-oscillation with key tracking" },
    { "pwm",       "Pulse with LFO pulse-width modulation" },
    { "sub",       "Sub oscillator focus, -2 oct narrow pulse mode" },
    { "arp",       "Arpeggiator up, 3 held notes, octave range 2" },
    { "sequencer", "8-step sequencer pattern with rests and ties" },
};
constexpr int kNumPatches = static_cast<int>(sizeof(kPatches) / sizeof(kPatches[0]));

// Each patch returns the parameter set plus (for arp/sequencer patches) a
// programming callback.
struct Patch {
    SH101Params params;
    std::vector<int> notes;        // notes to play in order
    double noteSeconds = 0.5;
    double gateFraction = 0.8;
    bool arp = false;
    std::vector<int> arpHeld;
    bool seq = false;
    std::vector<std::array<int, 3>> seqSteps;  // {note, gate, tie}
};

Patch makePatch(const std::string& name) {
    Patch p;
    SH101Params& q = p.params;

    if (name == "init") {
        q.sawLevel = 1.0; q.pulseLevel = 0.0; q.subLevel = 0.0; q.noiseLevel = 0.0;
        q.cutoff = 6000.0; q.resonance = 0.0; q.filterEnvAmt = 0.0; q.keyTrack = 0.0;
        q.attack = 0.005; q.decay = 0.6; q.sustain = 0.8; q.release = 0.2;
        p.notes = { 48, 55, 60, 55 };
    } else if (name == "bass") {
        q.sawLevel = 1.0; q.pulseLevel = 0.0; q.subLevel = 0.7; q.subMode = 0;
        q.cutoff = 300.0; q.resonance = 0.35; q.filterEnvAmt = 2.5; q.keyTrack = 0.3;
        q.attack = 0.002; q.decay = 0.35; q.sustain = 0.25; q.release = 0.15;
        q.envTrigger = 0; q.vcaMode = 0;
        p.notes = { 36, 36, 43, 34 };
        p.noteSeconds = 0.4;
    } else if (name == "pluck") {
        q.sawLevel = 1.0; q.cutoff = 400.0; q.resonance = 0.5;
        q.filterEnvAmt = 3.5; q.keyTrack = 0.4; q.decay = 0.25; q.sustain = 0.0;
        q.release = 0.15; q.attack = 0.0015;
        p.notes = { 60, 63, 67, 70 };
        p.noteSeconds = 0.35;
        p.gateFraction = 0.5;
    } else if (name == "selfosc") {
        q.sawLevel = 0.0; q.pulseLevel = 0.0; q.noiseLevel = 0.06;
        q.cutoff = 200.0; q.resonance = 1.0; q.keyTrack = 1.0;
        q.filterEnvAmt = 0.0; q.attack = 0.01; q.decay = 3.0; q.sustain = 1.0; q.release = 0.4;
        p.notes = { 48, 55, 60 };
        p.noteSeconds = 1.0;
    } else if (name == "pwm") {
        q.pulseLevel = 1.0; q.sawLevel = 0.0; q.subLevel = 0.3;
        q.pulseWidth = 0.5; q.pwmSource = 2; q.pwmAmount = 0.8;
        q.lfoRate = 0.6; q.lfoWave = SH101LFO::Triangle;
        q.cutoff = 2500.0; q.resonance = 0.15; q.keyTrack = 0.2;
        q.attack = 0.15; q.decay = 1.0; q.sustain = 0.8; q.release = 0.5;
        p.notes = { 45, 52, 57 };
        p.noteSeconds = 1.0;
    } else if (name == "sub") {
        q.sawLevel = 0.25; q.pulseLevel = 0.0; q.subLevel = 1.0; q.subMode = 2;
        q.cutoff = 900.0; q.resonance = 0.2; q.filterEnvAmt = 1.0;
        q.attack = 0.004; q.decay = 0.5; q.sustain = 0.6; q.release = 0.3;
        p.notes = { 36, 41, 43, 36 };
        p.noteSeconds = 0.6;
    } else if (name == "arp") {
        q.sawLevel = 1.0; q.subLevel = 0.4; q.cutoff = 1500.0; q.resonance = 0.3;
        q.filterEnvAmt = 2.0; q.decay = 0.2; q.sustain = 0.2; q.release = 0.1;
        q.arpOn = true; q.arpMode = Arpeggiator::Up; q.arpRate = 8.0; q.arpOctaves = 2;
        p.arp = true;
        p.arpHeld = { 48, 52, 55 };
        p.noteSeconds = 2.0;
    } else if (name == "sequencer") {
        q.pulseLevel = 1.0; q.sawLevel = 0.5; q.subLevel = 0.5;
        q.cutoff = 700.0; q.resonance = 0.4; q.filterEnvAmt = 2.0;
        q.decay = 0.18; q.sustain = 0.1; q.release = 0.1;
        q.seqOn = true; q.seqRate = 6.0;
        p.seq = true;
        p.seqSteps = {
            { 36, 1, 0 }, { 48, 1, 0 }, { 36, 1, 1 }, { 43, 0, 0 },
            { 36, 1, 0 }, { 55, 1, 0 }, { 51, 1, 0 }, { 48, 1, 1 },
        };
        p.noteSeconds = 3.0;
    } else {
        // Unknown name -> init, but keep going so the CLI stays scriptable.
        return makePatch("init");
    }
    return p;
}

void setDefaultsForRender(SH101Params& q) {
    q.volume = 0.8;
    q.portamentoTime = 0.0;
    q.portamentoMode = Portamento::Off;
    q.lfoWave = SH101LFO::Triangle;
}

} // namespace

int main(int argc, char** argv) {
    std::string patchName = "init";
    std::string outPath;
    double seconds = 0.0;
    double sampleRate = 48000.0;
    int oversample = 2;
    int presetIndex = -1;          // -1 = use --patch instead

    for (int i = 1; i < argc; ++i) {
        const std::string a = argv[i];
        if (a == "--list") {
            std::printf("patches:\n");
            for (int i2 = 0; i2 < kNumPatches; ++i2) {
                std::printf("  %-10s %s\n", kPatches[i2].name, kPatches[i2].description);
            }
            return 0;
        } else if (a == "--list-presets") {
            const int count = numPresets();
            std::printf("presets (%d):\n", count);
            for (int i2 = 0; i2 < count; ++i2) {
                const PresetInfo& p = preset(i2);
                std::printf("  %2d  %-6s %-18s %s\n", i2, p.category, p.name, p.description);
            }
            return 0;
        } else if (a == "--preset" && i + 1 < argc) {
            const std::string arg = argv[++i];
            presetIndex = findPresetByName(arg.c_str());
            if (presetIndex < 0 && !arg.empty() && (arg[0] >= '0' && arg[0] <= '9')) {
                const int asIndex = std::atoi(arg.c_str());
                if (asIndex >= 0 && asIndex < numPresets()) presetIndex = asIndex;
            }
            if (presetIndex < 0) {
                std::printf("error: unknown preset '%s' (see --list-presets)\n", arg.c_str());
                return 2;
            }
        } else if (a == "--patch" && i + 1 < argc) {
            patchName = argv[++i];
        } else if (a == "--seconds" && i + 1 < argc) {
            seconds = std::atof(argv[++i]);
        } else if (a == "--sr" && i + 1 < argc) {
            sampleRate = std::atof(argv[++i]);
        } else if (a == "--os" && i + 1 < argc) {
            oversample = std::atoi(argv[++i]);
        } else if (a == "--help" || a == "-h") {
            std::printf("usage: sh101_render [--patch NAME] [--seconds S] [--sr HZ] [--os 1|2|4] out.wav\n"
                        "       sh101_render --preset NAME|INDEX [--seconds S] out.wav\n"
                        "       sh101_render --list | --list-presets\n");
            return 0;
        } else if (!a.empty() && a[0] != '-') {
            outPath = a;
        } else {
            std::printf("unknown option: %s\n", a.c_str());
            return 2;
        }
    }

    if (outPath.empty()) {
        std::printf("error: no output file given (see --help)\n");
        return 2;
    }

    Patch patch;
    if (presetIndex >= 0) {
        // A preset states its own control positions, so the generic render
        // defaults must not overwrite them.
        const PresetInfo& info = preset(presetIndex);
        patch.params = makePresetParams(presetIndex);

        if (patch.params.arpOn) {
            patch.arp = true;
            patch.arpHeld = { 48, 52, 55 };
            patch.noteSeconds = 2.0;
        } else if (patch.params.seqOn) {
            patch.seq = true;
            patch.noteSeconds = 2.0;
        } else {
            patch.notes = { 36, 48, 60, 48 };
            patch.noteSeconds = 0.5;
        }
        if (info.applySequence == nullptr && !patch.seq) {
            patch.seqSteps.clear();
        }
    } else {
        patch = makePatch(patchName);
        setDefaultsForRender(patch.params);
    }

    SH101Engine engine;
    engine.prepare(sampleRate, oversample);
    engine.setDeterministicTestMode(true, 0x5eed1234u);
    engine.setParams(patch.params);

    if (patch.seq) {
        void (*applySequence)(StepSequencer&) =
            (presetIndex >= 0) ? preset(presetIndex).applySequence : nullptr;
        if (applySequence != nullptr) {
            // The preset carries its own pattern (the sequence memory, not a panel
            // control), so the pattern comes from the preset rather than the CLI.
            applySequence(engine.sequencer());
        } else {
            engine.sequencer().setLength(static_cast<int>(patch.seqSteps.size()));
            for (size_t i = 0; i < patch.seqSteps.size(); ++i) {
                engine.sequencer().setStep(static_cast<int>(i), patch.seqSteps[i][0],
                                           patch.seqSteps[i][1] != 0, patch.seqSteps[i][2] != 0);
            }
        }
    }

    std::vector<float> audio;
    const int blockSize = 512;
    float block[blockSize];

    auto renderSeconds = [&](double secs) {
        const int total = static_cast<int>(secs * sampleRate);
        int done = 0;
        while (done < total) {
            const int n = std::min(blockSize, total - done);
            engine.renderBlock(block, n);
            audio.insert(audio.end(), block, block + n);
            done += n;
        }
    };

    if (patch.arp) {
        for (int n : patch.arpHeld) engine.noteOn(n, 1.0);
        renderSeconds(seconds > 0.0 ? seconds : patch.noteSeconds);
        for (int n : patch.arpHeld) engine.noteOff(n);
        renderSeconds(0.3);
    } else if (patch.seq) {
        engine.noteOn(60, 1.0);      // hold a key: the sequencer runs while gated
        renderSeconds(seconds > 0.0 ? seconds : patch.noteSeconds);
        engine.noteOff(60);
        renderSeconds(0.3);
    } else {
        const double noteSecs = patch.noteSeconds;
        for (int n : patch.notes) {
            engine.noteOn(n, 1.0);
            renderSeconds(noteSecs * patch.gateFraction);
            engine.noteOff(n);
            renderSeconds(noteSecs * (1.0 - patch.gateFraction));
        }
    }

    if (seconds > 0.0 && !patch.arp && !patch.seq) {
        // Keep the requested length: trim or pad with silence.
        const size_t want = static_cast<size_t>(seconds * sampleRate);
        if (audio.size() > want) audio.resize(want);
        else audio.resize(want, 0.0f);
    }

    if (!WavWriter::write(outPath, audio, static_cast<int>(sampleRate))) {
        std::printf("error: could not write %s\n", outPath.c_str());
        return 1;
    }

    double peak = 0.0;
    for (float s : audio) peak = std::max(peak, std::fabs(static_cast<double>(s)));
    std::printf("wrote %s: %s=%s, %.2f s @ %.0f Hz, oversample=%dx, peak=%.3f, safetyResets=%d\n",
                outPath.c_str(), presetIndex >= 0 ? "preset" : "patch",
                presetIndex >= 0 ? preset(presetIndex).name : patchName.c_str(),
                static_cast<double>(audio.size()) / sampleRate, sampleRate, oversample, peak,
                engine.safetyResetCount());
    return 0;
}
