// Preset file format (save / load) validation.
//
// A saved preset is a promise: it must come back exactly as it went out, on a
// later build, without the values drifting.  These tests cover that promise, plus
// the parts that make the format safe to evolve: unknown keys are ignored, missing
// keys fall back to the reference patch, and a file that is not one of ours is
// refused rather than silently loading as "some patch".
#include <cmath>
#include <cstdio>
#include <string>

#include "TestFramework.h"
#include "sh101/Params.h"
#include "sh101/PresetIO.h"
#include "sh101/Presets.h"

using namespace sh101;
using namespace sh101test;

namespace {

bool sameParameters(const SH101Params& a, const SH101Params& b) {
    for (int id = 0; id < kNumParams; ++id) {
        if (std::fabs(getNormalized(a, id) - getNormalized(b, id)) > 1.0e-12) return false;
    }
    return true;
}

} // namespace

SH101_TEST(preset_io_round_trips_every_factory_patch) {
    const int count = numPresets();
    for (int i = 0; i < count; ++i) {
        const PresetDocument source = documentFromBank(i);
        const std::string text = serialisePreset(source);

        PresetDocument loaded;
        if (!parsePreset(text, loaded)) {
            std::printf("  preset '%s' failed to parse back\n", source.name.c_str());
            CHECK(false);
            continue;
        }

        CHECK(loaded.name == source.name);
        CHECK(loaded.category == source.category);
        CHECK(loaded.description == source.description);
        if (!sameParameters(loaded.params, source.params)) {
            std::printf("  preset '%s' changed on the round-trip\n", source.name.c_str());
            CHECK(false);
        }
        CHECK(true);
    }
}

SH101_TEST(preset_io_round_trips_a_sequence_pattern) {
    PresetDocument doc;
    doc.name = "Pattern Test";
    doc.category = "User";

    StepSequencer sequencer;
    preset(findPresetByName("Acid Sequence")).applySequence(sequencer);
    StepSequencer::Step steps[StepSequencer::kMaxSteps];
    for (int i = 0; i < sequencer.length(); ++i) steps[i] = sequencer.step(i);
    doc.setPattern(steps, sequencer.length());

    PresetDocument loaded;
    CHECK(parsePreset(serialisePreset(doc), loaded));
    CHECK(loaded.stepCount == sequencer.length());
    for (int i = 0; i < loaded.stepCount; ++i) {
        CHECK(loaded.steps[i].note == steps[i].note);
        CHECK(loaded.steps[i].gate == steps[i].gate);
        CHECK(loaded.steps[i].tie == steps[i].tie);
    }
}

// A file written by a future build (extra keys) or an older one (missing keys)
// must still load: the parameters it does carry are applied, the rest stay at the
// reference patch.
SH101_TEST(preset_io_tolerates_unknown_and_missing_keys) {
    PresetDocument origin;
    origin.name = "Partial";
    origin.params.cutoff = 1234.0;
    origin.params.resonance = 0.6;

    std::string text = serialisePreset(origin);
    text += "somethingFromTheFuture=42\n";
    text += "anotherUnknown=hello world\n";

    PresetDocument loaded;
    CHECK(parsePreset(text, loaded));
    CHECK(loaded.name == "Partial");
    CHECK_NEAR(loaded.params.cutoff, 1234.0, 1.0e-6);
    CHECK_NEAR(loaded.params.resonance, 0.6, 1.0e-9);

    // Strip a line: the missing control must fall back, not read as zero.
    const std::string keyLine = std::string("cutoff=");
    const size_t start = text.find(keyLine);
    const size_t end = text.find('\n', start);
    std::string missing = text.substr(0, start) + text.substr(end + 1);

    PresetDocument partiallyMissing;
    CHECK(parsePreset(missing, partiallyMissing));
    CHECK_NEAR(partiallyMissing.params.cutoff, SH101Params{}.cutoff, 1.0e-6);
    CHECK_NEAR(partiallyMissing.params.resonance, 0.6, 1.0e-9);
}

SH101_TEST(preset_io_refuses_foreign_files) {
    PresetDocument loaded;
    CHECK(!parsePreset("", loaded));
    CHECK(!parsePreset("this is not a preset file at all\n", loaded));
    CHECK(!parsePreset("<xml><some>host state</some></xml>\n", loaded));
    // A comment-only file is still not a preset.
    CHECK(!parsePreset("# just a comment\n", loaded));
}

SH101_TEST(preset_io_file_names_are_safe) {
    // The library stores one file per preset, named after the preset, so the name
    // has to survive a file system.
    CHECK(presetFileName("Acid Bass") == "Acid Bass");
    CHECK(presetFileName("My/Patch: v2*") == "My_Patch_ v2_");
    CHECK(presetFileName("  spaced   out  ") == "spaced out");
    CHECK(presetFileName("") == "Untitled");
    CHECK(presetFileName("...") == "...");
    // Non-ASCII names keep their characters: the instrument's own name is one.
    CHECK(presetFileName("Tråd & Trummor") == "Tråd _ Trummor");
    CHECK(presetFileName("ÖstraTorn101 Bass") == "ÖstraTorn101 Bass");
}

// The saved file has to be readable by a human and carry the identity of what
// wrote it — that is the point of the text format.
SH101_TEST(preset_io_file_is_human_readable) {
    const PresetDocument doc = documentFromBank(findPresetByName("Acid Bass"));
    const std::string text = serialisePreset(doc);

    CHECK(text.find("name=Acid Bass") != std::string::npos);
    CHECK(text.find("category=Bass") != std::string::npos);
    CHECK(text.find("format=1") != std::string::npos);
    // Parameter lines are keyed by name, not by index, so the file stays readable
    // and a re-ordered parameter enum cannot silently corrupt old presets.
    CHECK(text.find("cutoff=") != std::string::npos);
    CHECK(text.find("resonance=") != std::string::npos);
    CHECK(text.find("p13_") == std::string::npos);
}
