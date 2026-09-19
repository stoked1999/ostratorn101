// Preset file format — reading and writing patches as text.
//
// Why a text format, and why here: a preset must outlive the build that wrote it.
// The plugin and the host both store parameter values as normalised 0..1 numbers
// (the taper to engineering units lives in Params.h), so a preset file stores the
// same normalised values, keyed by parameter *name*.  That makes a file:
//
//   * readable and diffable by a human,
//   * independent of the calibration constants (a later fidelity milestone can
//     re-fit them without invalidating saved patches),
//   * testable without JUCE — this header needs nothing but the standard library,
//     so the engine test suite covers round-tripping, and the plugin only has to
//     supply a directory and a file.
//
// Unknown keys are ignored and missing keys fall back to the reference patch, so
// a file written by a newer or older build still loads.
#pragma once

#include <algorithm>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <string>

#include "sh101/Params.h"
#include "sh101/Presets.h"
#include "sh101/StepSequencer.h"

namespace sh101 {

struct PresetDocument {
    std::string name = "Untitled";
    std::string category = "User";
    std::string description;
    SH101Params params{};
    StepSequencer::Step steps[StepSequencer::kMaxSteps] = {};
    int stepCount = 0;

    void setPattern(const StepSequencer::Step* source, int count) {
        stepCount = clampi(count, 0, StepSequencer::kMaxSteps);
        for (int i = 0; i < stepCount; ++i) steps[i] = source[i];
    }
};

constexpr int kPresetFormatVersion = 1;

namespace detail {

inline std::string trim(const std::string& s) {
    const char* whitespace = " \t\r\n";
    const size_t first = s.find_first_not_of(whitespace);
    if (first == std::string::npos) return {};
    const size_t last = s.find_last_not_of(whitespace);
    return s.substr(first, last - first + 1);
}

// Parameter name -> id, or -1.
inline int paramIdFromName(const std::string& name) {
    for (int i = 0; i < kNumParams; ++i) {
        if (name == paramName(i)) return i;
    }
    return -1;
}

} // namespace detail

// Serialises a document.  Values are written with full double precision so a
// save/load cycle is lossless.
inline std::string serialisePreset(const PresetDocument& doc) {
    std::string out;
    out.reserve(2048);

    char line[256];
    out += "# ÖstraTorn101 preset\n";
    std::snprintf(line, sizeof(line), "format=%d\n", kPresetFormatVersion);
    out += line;
    out += "name=" + doc.name + "\n";
    out += "category=" + doc.category + "\n";
    out += "description=" + doc.description + "\n";

    for (int id = 0; id < kNumParams; ++id) {
        std::snprintf(line, sizeof(line), "%.17g", getNormalized(doc.params, id));
        out += std::string(paramName(id)) + "=" + line + "\n";
    }

    std::snprintf(line, sizeof(line), "seq_length=%d\n", doc.stepCount);
    out += line;
    for (int i = 0; i < doc.stepCount; ++i) {
        std::snprintf(line, sizeof(line), "seq_step_%d=%d,%d,%d\n", i, doc.steps[i].note,
                      doc.steps[i].gate ? 1 : 0, doc.steps[i].tie ? 1 : 0);
        out += line;
    }
    return out;
}

inline bool parsePreset(const std::string& text, PresetDocument& out) {
    // Start from the reference patch so missing keys have a defined meaning.
    PresetDocument parsed;
    parsed.params = SH101Params{};

    double normalized[kNumParams];
    for (int i = 0; i < kNumParams; ++i) normalized[i] = getNormalized(parsed.params, i);

    bool sawName = false;
    size_t position = 0;
    while (position <= text.size()) {
        const size_t end = text.find('\n', position);
        const std::string rawLine = text.substr(position, (end == std::string::npos) ? std::string::npos
                                                                                   : end - position);
        position = (end == std::string::npos) ? text.size() + 1 : end + 1;

        const std::string line = detail::trim(rawLine);
        if (line.empty() || line[0] == '#') continue;

        const size_t equals = line.find('=');
        if (equals == std::string::npos) continue;
        const std::string key = detail::trim(line.substr(0, equals));
        const std::string value = detail::trim(line.substr(equals + 1));

        if (key == "name") {
            parsed.name = value.empty() ? "Untitled" : value;
            sawName = true;
        } else if (key == "category") {
            parsed.category = value.empty() ? "User" : value;
        } else if (key == "description") {
            parsed.description = value;
        } else if (key == "seq_length") {
            parsed.stepCount = clampi(std::atoi(value.c_str()), 0, StepSequencer::kMaxSteps);
        } else if (key.compare(0, 9, "seq_step_") == 0) {
            const int index = std::atoi(key.c_str() + 9);
            if (index < 0 || index >= StepSequencer::kMaxSteps) continue;
            int note = 60;
            int gate = 1;
            int tie = 0;
            if (std::sscanf(value.c_str(), "%d,%d,%d", &note, &gate, &tie) != 3) continue;
            parsed.steps[index].note = clampi(note, 0, 127);
            parsed.steps[index].gate = (gate != 0);
            parsed.steps[index].tie = (tie != 0);
            parsed.stepCount = std::max(parsed.stepCount, index + 1);
        } else {
            const int id = detail::paramIdFromName(key);
            if (id >= 0) {
                normalized[id] = clampd(std::atof(value.c_str()), 0.0, 1.0);
            }
        }
    }

    // Nothing that looks like one of our files: refuse rather than load a patch
    // that is entirely the reference patch by accident.
    if (!sawName) return false;

    for (int id = 0; id < kNumParams; ++id) applyNormalized(parsed.params, id, normalized[id]);
    parsed.stepCount = clampi(parsed.stepCount, 0, StepSequencer::kMaxSteps);
    out = parsed;
    return true;
}

// The factory bank entry as a document (used when loading a factory preset and as
// the starting point of a user preset).
inline PresetDocument documentFromBank(int index) {
    PresetDocument doc;
    const PresetInfo& info = preset(index);
    doc.name = info.name;
    doc.category = info.category;
    doc.description = info.description;
    doc.params = makePresetParams(index);

    // A sequenced preset carries its pattern in the sequence memory rather than in
    // a parameter, so the document has to carry it too — otherwise loading the
    // preset through the document path (which is what the plugin does) would lose
    // the one thing that makes the preset what it is.
    if (info.applySequence != nullptr) {
        StepSequencer sequencer;
        info.applySequence(sequencer);
        StepSequencer::Step steps[StepSequencer::kMaxSteps];
        for (int i = 0; i < sequencer.length(); ++i) steps[i] = sequencer.step(i);
        doc.setPattern(steps, sequencer.length());
    }
    return doc;
}

// A document for the current parameters, for "save what I am hearing".
inline PresetDocument documentFromParams(const SH101Params& params, const std::string& name,
                                         const std::string& category) {
    PresetDocument doc;
    doc.name = name;
    doc.category = category;
    doc.params = params;
    return doc;
}

// File-name-safe version of a preset name (the library stores one file per
// preset, named after it).  Only the characters a file system actually rejects
// are replaced: high bytes are left alone, because a preset called "Tråd" must
// keep its name.
inline std::string presetFileName(const std::string& name) {
    std::string safe;
    safe.reserve(name.size());
    for (unsigned char c : name) {
        const bool asciiOk = (c >= 'a' && c <= 'z') || (c >= 'A' && c <= 'Z') || (c >= '0' && c <= '9')
                             || c == ' ' || c == '-' || c == '_' || c == '.' || c == '(' || c == ')'
                             || c == '+';
        const bool utf8Continuation = (c >= 0x80);   // part of a multi-byte character
        safe.push_back((asciiOk || utf8Continuation) ? static_cast<char>(c) : '_');
    }
    // Collapse runs of spaces and trim, so "  My  Patch " becomes "My Patch".
    std::string collapsed;
    bool lastSpace = false;
    for (char c : safe) {
        const bool space = (c == ' ');
        if (space && lastSpace) continue;
        collapsed.push_back(c);
        lastSpace = space;
    }
    while (!collapsed.empty() && collapsed.back() == ' ') collapsed.pop_back();
    while (!collapsed.empty() && collapsed.front() == ' ') collapsed.erase(collapsed.begin());
    if (collapsed.empty()) collapsed = "Untitled";
    return collapsed;
}

} // namespace sh101
