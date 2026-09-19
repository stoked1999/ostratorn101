// ÖstraTorn101 — VST3/Standalone wrapper: implementation.
#include "PluginProcessor.h"

#include <cmath>

#include "PluginEditor.h"
#include "sh101/Params.h"
#include "sh101/Presets.h"

namespace {

// The product name is not ASCII.  juce::String(const char*) interprets its input
// as Latin-1, so the UTF-8 bytes are decoded explicitly — otherwise the panel (and
// the name a host shows) reads "Ã–straTorn101".  JucePlugin_DisplayName is built as
// a C escape sequence so the bytes survive the compiler command line.
juce::String projectName() { return juce::String::fromUTF8(JucePlugin_DisplayName); }

const char* const kPresetExtension = ".otp101";

// A short, stable identifier per parameter (hosts persist these).
juce::String idForParam(int paramId) {
    return juce::String("p") + juce::String(paramId) + "_" + sh101::paramName(paramId);
}

juce::String fromUtf8(const std::string& text) {
    return juce::String::fromUTF8(text.c_str(), static_cast<int>(text.size()));
}

} // namespace

const std::vector<juce::String>& OstraTornAudioProcessor::parameterIds() {
    static const std::vector<juce::String> ids = [] {
        std::vector<juce::String> v;
        for (int i = 0; i < sh101::kNumParams; ++i) v.push_back(idForParam(i));
        return v;
    }();
    return ids;
}

juce::String OstraTornAudioProcessor::parameterId(int paramId) {
    const auto& ids = parameterIds();
    return ids[static_cast<size_t>(juce::jlimit(0, sh101::kNumParams - 1, paramId))];
}

juce::String OstraTornAudioProcessor::parameterName(int paramId) {
    return juce::String(sh101::paramName(paramId));
}

juce::AudioProcessorValueTreeState::ParameterLayout
OstraTornAudioProcessor::createParameterLayout() {
    juce::AudioProcessorValueTreeState::ParameterLayout layout;
    // Every control is exposed at its normalized position: continuous controls as
    // 0..1 floats, switch-like controls as choices with their real position
    // names.  Defaults come from the engine's reference patch, so a freshly
    // loaded instance is immediately playable.
    const sh101::SH101Params defaultPatch{};
    for (int i = 0; i < sh101::kNumParams; ++i) {
        const juce::String id = parameterId(i);
        const juce::String name = parameterName(i);
        const double defaultNormalized = sh101::getNormalized(defaultPatch, i);

        int itemCount = 0;
        const char* const* items = sh101::paramChoiceItems(i, itemCount);
        if (items != nullptr && itemCount > 1) {
            juce::StringArray choices;
            for (int k = 0; k < itemCount; ++k) choices.add(items[k]);
            const int defaultIndex = juce::jlimit(
                0, itemCount - 1, static_cast<int>(std::lround(defaultNormalized * (itemCount - 1))));
            layout.add(std::make_unique<juce::AudioParameterChoice>(
                juce::ParameterID{ id, 1 }, name, choices, defaultIndex));
        } else {
            layout.add(std::make_unique<juce::AudioParameterFloat>(
                juce::ParameterID{ id, 1 }, name, juce::NormalisableRange<float>(0.0f, 1.0f),
                static_cast<float>(defaultNormalized)));
        }
    }
    return layout;
}

OstraTornAudioProcessor::OstraTornAudioProcessor()
    : juce::AudioProcessor(BusesProperties().withOutput("Output", juce::AudioChannelSet::stereo(),
                                                       true)),
      apvts_(*this, nullptr, "OstraTorn101", createParameterLayout()) {
    for (int i = 0; i < sh101::kNumParams; ++i) {
        parameterPointers_[static_cast<size_t>(i)] = apvts_.getRawParameterValue(parameterId(i));

        int itemCount = 0;
        const bool choice = sh101::paramChoiceItems(i, itemCount) != nullptr && itemCount > 1;
        rawToNormalized_[static_cast<size_t>(i)] =
            choice ? (1.0f / static_cast<float>(itemCount - 1)) : 1.0f;
    }
    loadPreset(0);   // the bank's reference patch, matching the parameter defaults

    float values[sh101::kNumParams] = {};
    readParameterSnapshot(values);
    adapter_.setParametersNormalized(values, sh101::kNumParams);
    adapter_.commitParameters();
}

// Reads every parameter at once, converted to the normalized form the engine's
// taper expects.  Used by both the audio callback and the message thread.
void OstraTornAudioProcessor::readParameterSnapshot(float* destination) const {
    for (int i = 0; i < sh101::kNumParams; ++i) {
        destination[i] = parameterPointers_[static_cast<size_t>(i)]->load()
                         * rawToNormalized_[static_cast<size_t>(i)];
    }
}

void OstraTornAudioProcessor::prepareToPlay(double sampleRate, int samplesPerBlock) {
    adapter_.prepare(sampleRate, samplesPerBlock, 2);   // 2x oversampled filter path
    float values[sh101::kNumParams] = {};
    readParameterSnapshot(values);
    adapter_.setParametersNormalized(values, sh101::kNumParams);
}

void OstraTornAudioProcessor::releaseResources() {
    adapter_.reset();
}

bool OstraTornAudioProcessor::isBusesLayoutSupported(const BusesLayout& layouts) const {
    // Mono or stereo out, no inputs: the instrument is monophonic and the model
    // does not synthesise a stereo image.
    const auto& out = layouts.getMainOutputChannelSet();
    if (out != juce::AudioChannelSet::mono() && out != juce::AudioChannelSet::stereo()) {
        return false;
    }
    return layouts.getMainInputChannelSet() == juce::AudioChannelSet::disabled();
}

// ---- Sequencer pattern hand-over -------------------------------------------
void OstraTornAudioProcessor::queuePattern(const sh101::PresetDocument& doc) {
    if (doc.stepCount <= 0) return;
    const juce::SpinLock::ScopedLockType lock(patternLock_);
    pendingPattern_ = doc;
    pendingPatternReady_.store(true, std::memory_order_release);
}

void OstraTornAudioProcessor::applyPendingPattern() {
    if (! pendingPatternReady_.load(std::memory_order_acquire)) return;
    // tryEnter only: an audio callback must never wait for the UI.
    const juce::SpinLock::ScopedTryLockType lock(patternLock_);
    if (! lock.isLocked()) return;

    auto& sequencer = adapter_.engine().sequencer();
    sequencer.setLength(pendingPattern_.stepCount);
    for (int i = 0; i < pendingPattern_.stepCount; ++i) {
        const auto& step = pendingPattern_.steps[i];
        sequencer.setStep(i, step.note, step.gate, step.tie);
    }
    pendingPatternReady_.store(false, std::memory_order_release);
}

void OstraTornAudioProcessor::processBlock(juce::AudioBuffer<float>& buffer,
                                          juce::MidiBuffer& midi) {
    juce::ScopedNoDenormals noDenormals;
    const int numSamples = buffer.getNumSamples();

    applyPendingPattern();

    // Automation: one parameter commit per block, converted to the normalized
    // values the engine's taper expects.
    float values[sh101::kNumParams] = {};
    readParameterSnapshot(values);
    adapter_.setParametersNormalized(values, sh101::kNumParams);

    // MIDI, in order, before rendering (an event inside the block must not be
    // lost — the adapter and engine handle events, not just state).
    for (const auto metadata : midi) {
        const auto message = metadata.getMessage();
        const int size = message.getRawDataSize();
        const juce::uint8* raw = message.getRawData();
        if (size >= 3) {
            adapter_.handleMidiMessage(raw[0], raw[1], raw[2]);
        } else if (size == 2) {
            adapter_.handleMidiMessage(raw[0], raw[1], 0);
        }
    }
    midi.clear();

    // Audio: render mono and copy to every output channel.
    float* const* channels = buffer.getArrayOfWritePointers();
    adapter_.renderBlock(channels, buffer.getNumChannels(), numSamples);

    float peak = 0.0f;
    for (int channel = 0; channel < buffer.getNumChannels(); ++channel) {
        peak = juce::jmax(peak, buffer.getMagnitude(channel, 0, numSamples));
    }

    if (standby_.load(std::memory_order_relaxed)) {
        // Standby: silence the output but keep the voice running, so leaving
        // standby does not restart the instrument mid-note.
        buffer.clear();
        peak = 0.0f;
    }
    outputLevel_.store(peak, std::memory_order_relaxed);
}

// ---- Presets ---------------------------------------------------------------
int OstraTornAudioProcessor::getNumPrograms() {
    return sh101::numPresets();
}

int OstraTornAudioProcessor::getCurrentProgram() {
    return currentPreset_.load(std::memory_order_relaxed);
}

void OstraTornAudioProcessor::setCurrentProgram(int index) {
    loadPreset(index);
}

const juce::String OstraTornAudioProcessor::getProgramName(int index) {
    const sh101::PresetInfo& info = sh101::preset(index);
    return juce::String(info.category) + ": " + info.name;
}

void OstraTornAudioProcessor::applyDocument(const sh101::PresetDocument& doc,
                                            const juce::String& name, bool isUser) {
    for (int i = 0; i < sh101::kNumParams; ++i) {
        if (auto* param = apvts_.getParameter(parameterId(i))) {
            // Notifying the host keeps the change visible to automation lanes and
            // to the host's own "modified" indicator.
            param->setValueNotifyingHost(static_cast<float>(sh101::getNormalized(doc.params, i)));
        }
    }
    queuePattern(doc);

    loadedParams_ = doc.params;
    currentPresetName_ = name;
    currentPresetIsUser_ = isUser;
}

void OstraTornAudioProcessor::loadPreset(int index) {
    const int count = sh101::numPresets();
    index = juce::jlimit(0, count - 1, index);
    applyDocument(sh101::documentFromBank(index), juce::String(sh101::preset(index).name), false);
    currentPreset_.store(index, std::memory_order_relaxed);
}

juce::File OstraTornAudioProcessor::userPresetDirectory() {
    return juce::File::getSpecialLocation(juce::File::userApplicationDataDirectory)
        .getChildFile(projectName())
        .getChildFile("Presets");
}

juce::File OstraTornAudioProcessor::userPresetFile(const juce::String& name) const {
    return userPresetDirectory()
        .getChildFile(juce::String(sh101::presetFileName(name.toStdString())) + kPresetExtension);
}

juce::StringArray OstraTornAudioProcessor::userPresetNames() const {
    juce::StringArray names;
    for (const auto& entry : juce::RangedDirectoryIterator(userPresetDirectory(), false,
                                                           juce::String("*") + kPresetExtension,
                                                           juce::File::findFiles)) {
        const juce::String baseName = entry.getFile().getFileNameWithoutExtension();

        // Prefer the name stored inside the file: it is what the user typed, and
        // it survives the file-name sanitising.
        sh101::PresetDocument doc;
        const juce::String text = entry.getFile().loadFileAsString();
        if (sh101::parsePreset(text.toStdString(), doc) && doc.name != "Untitled") {
            names.add(fromUtf8(doc.name));
        } else {
            names.add(baseName);
        }
    }
    names.sort(true);
    return names;
}

bool OstraTornAudioProcessor::captureDocument(sh101::PresetDocument& out) const {
    sh101::PresetDocument doc;
    doc.name = currentPresetName_.isEmpty() ? "Untitled" : currentPresetName_.toStdString();
    doc.category = "User";
    doc.description = "Saved from the panel";

    for (int i = 0; i < sh101::kNumParams; ++i) {
        const float value = parameterPointers_[static_cast<size_t>(i)]->load()
                             * rawToNormalized_[static_cast<size_t>(i)];
        sh101::applyNormalized(doc.params, i, value);
    }

    // The sequencer memory is not a parameter, so it is captured here; otherwise
    // a saved preset would lose the pattern it was built around.
    const auto& sequencer = adapter_.engine().sequencer();
    sh101::StepSequencer::Step steps[sh101::StepSequencer::kMaxSteps];
    for (int i = 0; i < sequencer.length(); ++i) steps[i] = sequencer.step(i);
    doc.setPattern(steps, sequencer.length());

    out = doc;
    return true;
}

bool OstraTornAudioProcessor::saveUserPreset(const juce::String& name) {
    sh101::PresetDocument doc;
    captureDocument(doc);
    doc.name = name.toStdString();
    doc.category = "User";
    doc.description = "Saved from the panel";

    const juce::File directory = userPresetDirectory();
    if (! directory.exists() && ! directory.createDirectory().wasOk()) return false;

    return userPresetFile(name).replaceWithText(fromUtf8(sh101::serialisePreset(doc)));
}

bool OstraTornAudioProcessor::loadUserPresetFile(const juce::File& file) {
    if (! file.existsAsFile()) return false;
    sh101::PresetDocument doc;
    if (! sh101::parsePreset(file.loadFileAsString().toStdString(), doc)) return false;

    applyDocument(doc, fromUtf8(doc.name), true);
    return true;
}

bool OstraTornAudioProcessor::loadUserPreset(const juce::String& name) {
    return loadUserPresetFile(userPresetFile(name));
}

bool OstraTornAudioProcessor::deleteUserPreset(const juce::String& name) {
    const juce::File file = userPresetFile(name);
    return file.existsAsFile() && file.deleteFile();
}

juce::String OstraTornAudioProcessor::currentPresetName() const {
    return currentPresetName_;
}

bool OstraTornAudioProcessor::presetIsModified() const {
    for (int i = 0; i < sh101::kNumParams; ++i) {
        // Compare normalized positions: a choice parameter's raw value is an item
        // index, so comparing it against a normalized target would always differ.
        const float value = parameterPointers_[static_cast<size_t>(i)]->load()
                             * rawToNormalized_[static_cast<size_t>(i)];
        if (std::fabs(value - sh101::getNormalized(loadedParams_, i)) > 1.0e-6) return true;
    }
    return false;
}

void OstraTornAudioProcessor::setStandby(bool on) {
    standby_.store(on, std::memory_order_relaxed);
    if (on) outputLevel_.store(0.0f, std::memory_order_relaxed);
}

// ---- Editor / state --------------------------------------------------------
juce::AudioProcessorEditor* OstraTornAudioProcessor::createEditor() {
    return new OstraTornAudioProcessorEditor(*this);
}

void OstraTornAudioProcessor::getStateInformation(juce::MemoryBlock& destData) {
    auto state = apvts_.copyState();

    // Everything the parameters cannot carry: which preset this is, whether it is
    // a user preset, standby, and the sequencer memory.
    state.setProperty("preset", currentPreset_.load(), nullptr);
    state.setProperty("presetName", currentPresetName_, nullptr);
    state.setProperty("presetIsUser", currentPresetIsUser_, nullptr);
    state.setProperty("standby", isStandby(), nullptr);

    sh101::PresetDocument doc;
    captureDocument(doc);
    state.setProperty("seqLength", doc.stepCount, nullptr);
    for (int i = 0; i < doc.stepCount; ++i) {
        state.setProperty(juce::Identifier("seq" + juce::String(i)),
                          juce::String(doc.steps[i].note) + ","
                              + juce::String(doc.steps[i].gate ? 1 : 0) + ","
                              + juce::String(doc.steps[i].tie ? 1 : 0),
                          nullptr);
    }

    if (auto xml = std::unique_ptr<juce::XmlElement>(state.createXml())) {
        copyXmlToBinary(*xml, destData);
    }
}

void OstraTornAudioProcessor::setStateInformation(const void* data, int sizeInBytes) {
    std::unique_ptr<juce::XmlElement> xml(getXmlFromBinary(data, sizeInBytes));
    if (xml == nullptr) return;

    const int presetIndex = xml->getIntAttribute("preset", 0);
    const juce::String presetName = xml->getStringAttribute("presetName");
    const bool isUser = xml->getBoolAttribute("presetIsUser", false);

    apvts_.replaceState(juce::ValueTree::fromXml(*xml));

    currentPreset_.store(juce::jlimit(0, sh101::numPresets() - 1, presetIndex),
                         std::memory_order_relaxed);
    currentPresetName_ = presetName.isEmpty()
                             ? juce::String(sh101::preset(presetIndex).name)
                             : presetName;
    currentPresetIsUser_ = isUser;
    setStandby(xml->getBoolAttribute("standby", false));

    // Restore the sequence memory the parameters cannot carry.
    sh101::PresetDocument doc;
    const int stepCount = juce::jlimit(0, sh101::StepSequencer::kMaxSteps,
                                       xml->getIntAttribute("seqLength", 0));
    for (int i = 0; i < stepCount; ++i) {
        const juce::String value = xml->getStringAttribute(juce::Identifier("seq" + juce::String(i)));
        int note = 60;
        int gate = 1;
        int tie = 0;
        if (std::sscanf(value.toStdString().c_str(), "%d,%d,%d", &note, &gate, &tie) == 3) {
            doc.steps[i].note = juce::jlimit(0, 127, note);
            doc.steps[i].gate = (gate != 0);
            doc.steps[i].tie = (tie != 0);
        }
    }
    doc.stepCount = stepCount;
    queuePattern(doc);

    loadedParams_ = sh101::SH101Params{};
    float snapshot[sh101::kNumParams] = {};
    readParameterSnapshot(snapshot);
    for (int i = 0; i < sh101::kNumParams; ++i) {
        sh101::applyNormalized(loadedParams_, i, snapshot[i]);
    }
}

juce::AudioProcessor* JUCE_CALLTYPE createPluginFilter() {
    return new OstraTornAudioProcessor();
}
