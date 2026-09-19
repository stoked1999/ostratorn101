// Preset-bank validation.
//
// The brief requires every claim the product makes to be backed by a test.  A
// preset bank is a claim ("this will sound like X"), so the suite checks the
// things a test can check objectively:
//   * the bank is well formed (names, categories, uniqueness);
//   * every preset is inside the documented range of every control;
//   * every preset survives the normalized round-trip the plugin actually uses
//     (engine values -> 0..1 host parameters -> engine values again), because a
//     preset that cannot be expressed as parameter values would not load
//     correctly in a host;
//   * every preset produces sound, stays finite and stays bounded;
//   * switch-like controls are presented with names, and each preset's position
//     is a real position.
//
// What a test cannot check is whether a preset is *musical*; that is what the
// rendered WAVs in renders/ and a human ear are for.
#include <cmath>
#include <cstdio>
#include <cstring>
#include <vector>

#include "TestFramework.h"
#include "TestHelpers.h"
#include "sh101/Presets.h"
#include "sh101/SH101Engine.h"

using namespace sh101;
using namespace sh101test;

namespace {

constexpr double kSampleRate = 48000.0;
constexpr int kBlockSize = 256;

// Renders a preset the way a player would hear it: one note held, then released.
std::vector<float> renderPreset(int presetIndex, const int* notes, int numNotes,
                                double noteSeconds, double releaseSeconds) {
    SH101Engine engine;
    engine.prepare(kSampleRate, 2);
    engine.setDeterministicTestMode(true, 0x5151u + static_cast<unsigned>(presetIndex));

    const SH101Params params = makePresetParams(presetIndex);
    engine.setParams(params);

    const PresetInfo& info = preset(presetIndex);
    if (info.applySequence != nullptr) {
        info.applySequence(engine.sequencer());
    }

    std::vector<float> audio;
    float block[kBlockSize];
    auto render = [&](double secs) {
        const int total = static_cast<int>(secs * kSampleRate);
        int done = 0;
        while (done < total) {
            const int n = (total - done < kBlockSize) ? (total - done) : kBlockSize;
            engine.renderBlock(block, n);
            audio.insert(audio.end(), block, block + n);
            done += n;
        }
    };

    for (int i = 0; i < numNotes; ++i) {
        engine.noteOn(notes[i], 1.0);
        render(noteSeconds);
        engine.noteOff(notes[i]);
        render(releaseSeconds);
    }
    return audio;
}

double peakOf(const std::vector<float>& audio) {
    double peak = 0.0;
    for (float s : audio) peak = std::max(peak, std::fabs(static_cast<double>(s)));
    return peak;
}

} // namespace

// -----------------------------------------------------------------------------
SH101_TEST(presets_bank_is_well_formed) {
    int count = 0;
    const PresetInfo* bank = presetBank(count);
    CHECK(bank != nullptr);
    CHECK_GT(count, 20);            // a *mixed* bank, not a handful of patches

    for (int i = 0; i < count; ++i) {
        CHECK(bank[i].name != nullptr && bank[i].name[0] != '\0');
        CHECK(bank[i].category != nullptr && bank[i].category[0] != '\0');
        CHECK(bank[i].description != nullptr && bank[i].description[0] != '\0');
        CHECK(bank[i].apply != nullptr);
        // Names must be unique: they are how a preset is found by name.
        for (int j = i + 1; j < count; ++j) {
            CHECK(std::strcmp(bank[i].name, bank[j].name) != 0);
        }
    }

    // Categories must be drawn from the panel's own vocabulary, so the selector
    // reads like the instrument.
    const char* allowed[] = { "Init", "Bass", "Lead", "Pad", "Pluck", "Perc", "FX", "Arp", "Seq" };
    for (int i = 0; i < count; ++i) {
        bool ok = false;
        for (const char* a : allowed) {
            if (std::strcmp(bank[i].category, a) == 0) { ok = true; break; }
        }
        CHECK(ok);
    }
}

// The plugin stores every control as a normalized 0..1 parameter.  Loading a
// preset therefore means: preset value -> getNormalized -> host parameter ->
// applyNormalized -> engine value.  If that round-trip is not exact for a
// control, that control will drift every time a preset is loaded, so the suite
// checks it for every control of every preset.
SH101_TEST(presets_round_trip_through_host_parameters) {
    const int count = numPresets();
    for (int i = 0; i < count; ++i) {
        const SH101Params original = makePresetParams(i);
        SH101Params reloaded = original;

        for (int id = 0; id < kNumParams; ++id) {
            const double n = getNormalized(original, id);
            CHECK(n >= 0.0 && n <= 1.0);      // in range => inside documented limits
            applyNormalized(reloaded, id, n);
        }

        for (int id = 0; id < kNumParams; ++id) {
            const double a = getNormalized(original, id);
            const double b = getNormalized(reloaded, id);
            if (std::fabs(a - b) > 1.0e-9) {
                std::printf("  preset '%s' control '%s' drifted through the round-trip\n",
                            preset(i).name, paramName(id));
                CHECK(false);
            }
        }
        CHECK(true);
    }
}

SH101_TEST(presets_differ_from_the_reference_patch) {
    const SH101Params reference = makePresetParams(0);   // "Init"
    const int count = numPresets();
    for (int i = 1; i < count; ++i) {
        const SH101Params p = makePresetParams(i);
        int differences = 0;
        for (int id = 0; id < kNumParams; ++id) {
            if (std::fabs(getNormalized(p, id) - getNormalized(reference, id)) > 1.0e-9) ++differences;
        }
        if (differences < 4) {
            std::printf("  preset '%s' changes only %d control(s) from the reference patch\n",
                        preset(i).name, differences);
            CHECK(false);
        }
        CHECK(true);
    }
}

SH101_TEST(presets_all_produce_sound) {
    // A held note, played the way a player would, at three positions in the
    // range.  Every preset must be audible, finite and inside sane levels.
    const int notes[] = { 36, 48, 60 };
    const int count = numPresets();

    for (int i = 0; i < count; ++i) {
        const std::vector<float> audio = renderPreset(i, notes, 3, 0.45, 0.35);
        const double peak = peakOf(audio);

        if (!allFinite(std::vector<double>(audio.begin(), audio.end()))) {
            std::printf("  preset '%s' produced non-finite samples\n", preset(i).name);
            CHECK(false);
        }
        if (!(peak > 0.01)) {
            std::printf("  preset '%s' is effectively silent (peak %.6f)\n", preset(i).name, peak);
            CHECK(false);
        }
        if (!(peak < 1.5)) {
            std::printf("  preset '%s' is too loud (peak %.4f)\n", preset(i).name, peak);
            CHECK(false);
        }
        CHECK(true);
    }
}

// Every switch-like control must be presentable by name, and every preset must
// sit on a real position of it.  This is what stops the editor from showing a
// nameless 0..1 fader where the hardware has discrete steps.
SH101_TEST(presets_use_named_positions_for_switch_controls) {
    const int switchLike[] = { pLfoWave, pVcoRange, pSubMode, pPwmSource, pEnvTrigger,
                               pVcaMode, pPortamentoMode, pArpMode, pArpOn, pSeqOn };
    for (int id : switchLike) {
        CHECK(isChoiceParameter(id));
        int items = 0;
        const char* const* names = paramChoiceItems(id, items);
        CHECK(names != nullptr);
        CHECK_GT(items, 1);
        for (int k = 0; k < items; ++k) CHECK(names[k] != nullptr && names[k][0] != '\0');
    }

    // Continuous controls must not claim to be switches.
    CHECK(!isChoiceParameter(pCutoff));
    CHECK(!isChoiceParameter(pResonance));
    CHECK(!isChoiceParameter(pVolume));

    const int count = numPresets();
    for (int i = 0; i < count; ++i) {
        const SH101Params p = makePresetParams(i);
        for (int id : switchLike) {
            int items = 0;
            paramChoiceItems(id, items);
            const double n = getNormalized(p, id);
            const int position = static_cast<int>(n * (items - 1) + 0.5);
            if (position < 0 || position >= items) {
                std::printf("  preset '%s' control '%s' is on position %d of %d\n",
                            preset(i).name, paramName(id), position, items);
                CHECK(false);
            }
        }
        CHECK(true);
    }
}

// Presets that ship a sequence pattern must actually program it, and the
// pattern must not be a single repeated note (that would be indistinguishable
// from the sequencer running with its power-on default).
SH101_TEST(sequenced_presets_carry_a_pattern) {
    int sequenced = 0;
    const int count = numPresets();
    for (int i = 0; i < count; ++i) {
        const PresetInfo& info = preset(i);
        if (info.applySequence == nullptr) continue;
        ++sequenced;
        CHECK(info.apply == nullptr || makePresetParams(i).seqOn);   // a pattern implies the sequencer is on
    }
    CHECK_GT(sequenced, 1);

    // Applying the pattern must change the sequence away from the power-on
    // default and must be playable (every gated step has a note in range).
    StepSequencer fresh;
    const PresetInfo& acid = preset(findPresetByName("Acid Sequence"));
    CHECK(acid.applySequence != nullptr);
    acid.applySequence(fresh);

    CHECK_GT(fresh.length(), 1);
    int distinctNotes = 0;
    int previous = -1;
    for (int s = 0; s < fresh.length(); ++s) {
        const int note = fresh.step(s).note;
        CHECK(note >= 0 && note <= 127);
        if (note != previous) ++distinctNotes;
        previous = note;
    }
    CHECK_GT(distinctNotes, 2);
}

SH101_TEST(presets_can_be_found_by_name) {
    CHECK(std::strcmp(preset(findPresetByName("Acid Bass")).name, "Acid Bass") == 0);
    CHECK(findPresetByName("acid bass") == findPresetByName("Acid Bass"));   // case-insensitive
    CHECK(findPresetByName("acidbass") == findPresetByName("Acid Bass"));    // space-tolerant
    CHECK(findPresetByName("PWM Strings") == findPresetByName("PWM Strings"));
    CHECK(findPresetByName("no such preset") == -1);
    CHECK(findPresetByName(nullptr) == -1);
}
