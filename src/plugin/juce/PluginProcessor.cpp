// JUCE VST3/Standalone wrapper: implementation.
#include "PluginProcessor.h"

#include <cmath>

#include "PluginEditor.h"
#include "sh101/Params.h"
#include "sh101/Presets.h"

namespace {
// A short, stable identifier per parameter (hosts persist these).
juce::String idForParam(int paramId) {
    return juce::String("p") + juce::String(paramId) + "_" + sh101::paramName(paramId);
}
} // namespace

const std::vector<juce::String>& SH101AudioProcessor::parameterIds() {
    static const std::vector<juce::String> ids = [] {
        std::vector<juce::String> v;
        for (int i = 0; i < sh101::kNumParams; ++i) v.push_back(idForParam(i));
        return v;
    }();
    return ids;
}

juce::String SH101AudioProcessor::parameterId(int paramId) {
    const auto& ids = parameterIds();
    return ids[static_cast<size_t>(juce::jlimit(0, sh101::kNumParams - 1, paramId))];
}

juce::String SH101AudioProcessor::parameterName(int paramId) {
    return juce::String(sh101::paramName(paramId));
}

juce::AudioProcessorValueTreeState::ParameterLayout SH101AudioProcessor::createParameterLayout() {
    juce::AudioProcessorValueTreeState::ParameterLayout layout;
    // Every control is exposed at its normalized position: continuous controls as
    // 0..1 floats, switch-like controls as choices with their real position
    // names.  The defaults come from the engine's own default patch
    // (SH101Params), so a freshly loaded instance is immediately playable and
    // matches the reference patch used by the tests and the renderer.  Defaulting
    // every parameter to 0 would load a silent instrument: all four source levels
    // and the volume are parameters here.
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

SH101AudioProcessor::SH101AudioProcessor()
    : juce::AudioProcessor(BusesProperties().withOutput("Output", juce::AudioChannelSet::stereo(),
                                                       true)),
      apvts_(*this, nullptr, "SH101", createParameterLayout()) {
    for (int i = 0; i < sh101::kNumParams; ++i) {
        parameterPointers_[static_cast<size_t>(i)] =
            apvts_.getRawParameterValue(parameterId(i));
    }
    // Start on the bank's reference patch, matching the parameter defaults.
    loadPreset(0);
    float values[sh101::kNumParams] = {};
    for (int i = 0; i < sh101::kNumParams; ++i) {
        values[i] = parameterPointers_[static_cast<size_t>(i)]->load();
    }
    adapter_.setParametersNormalized(values, sh101::kNumParams);
    adapter_.commitParameters();
}

void SH101AudioProcessor::prepareToPlay(double sampleRate, int samplesPerBlock) {
    adapter_.prepare(sampleRate, samplesPerBlock, 2);   // 2x oversampled filter path
    // Push the current automation into the freshly prepared engine.
    float values[sh101::kNumParams] = {};
    for (int i = 0; i < sh101::kNumParams; ++i) {
        values[i] = parameterPointers_[static_cast<size_t>(i)]->load();
    }
    adapter_.setParametersNormalized(values, sh101::kNumParams);
}

void SH101AudioProcessor::releaseResources() {
    adapter_.reset();
}

bool SH101AudioProcessor::isBusesLayoutSupported(const BusesLayout& layouts) const {
    // Mono or stereo out, no inputs: the SH-101 is a monophonic instrument and
    // the model does not synthesise a stereo image.
    const auto& out = layouts.getMainOutputChannelSet();
    if (out != juce::AudioChannelSet::mono() && out != juce::AudioChannelSet::stereo()) {
        return false;
    }
    return layouts.getMainInputChannelSet() == juce::AudioChannelSet::disabled();
}

// Applied on the audio thread: the sequence memory has exactly one writer.
void SH101AudioProcessor::applyPendingPattern() {
    const int pending = pendingPatternIndex_.exchange(0, std::memory_order_acq_rel);
    if (pending <= 0) return;
    const sh101::PresetInfo& info = sh101::preset(pending - 1);
    if (info.applySequence != nullptr) {
        info.applySequence(adapter_.engine().sequencer());
    }
}

void SH101AudioProcessor::processBlock(juce::AudioBuffer<float>& buffer,
                                      juce::MidiBuffer& midi) {
    juce::ScopedNoDenormals noDenormals;
    const int numSamples = buffer.getNumSamples();

    // 0. A preset loaded since the last block may carry a sequence pattern.
    applyPendingPattern();

    // 1. Automation: one parameter commit per block.
    float values[sh101::kNumParams] = {};
    for (int i = 0; i < sh101::kNumParams; ++i) {
        values[i] = parameterPointers_[static_cast<size_t>(i)]->load();
    }
    adapter_.setParametersNormalized(values, sh101::kNumParams);

    // 2. MIDI, in order, before rendering (an event inside the block must not be
    //    lost — the adapter and engine handle events, not just state).
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

    // 3. Audio: render mono and copy to every output channel.
    float* const* channels = buffer.getArrayOfWritePointers();
    adapter_.renderBlock(channels, buffer.getNumChannels(), numSamples);
}

// ---- Presets / programs ----------------------------------------------------
int SH101AudioProcessor::getNumPrograms() {
    return sh101::numPresets();
}

int SH101AudioProcessor::getCurrentProgram() {
    return currentPreset_.load(std::memory_order_relaxed);
}

void SH101AudioProcessor::setCurrentProgram(int index) {
    loadPreset(index);
}

const juce::String SH101AudioProcessor::getProgramName(int index) {
    const sh101::PresetInfo& info = sh101::preset(index);
    return juce::String(info.category) + ": " + info.name;
}

void SH101AudioProcessor::loadPreset(int index) {
    const int count = sh101::numPresets();
    index = juce::jlimit(0, count - 1, index);
    const sh101::SH101Params params = sh101::makePresetParams(index);

    for (int i = 0; i < sh101::kNumParams; ++i) {
        if (auto* param = apvts_.getParameter(parameterId(i))) {
            // Notifying the host keeps the change visible to automation lanes and
            // to the host's own "modified" indicator.
            param->setValueNotifyingHost(static_cast<float>(sh101::getNormalized(params, i)));
        }
    }

    if (sh101::preset(index).applySequence != nullptr) {
        pendingPatternIndex_.store(index + 1, std::memory_order_release);
    }
    currentPreset_.store(index, std::memory_order_relaxed);
}

juce::AudioProcessorEditor* SH101AudioProcessor::createEditor() {
    return new SH101AudioProcessorEditor(*this);
}

void SH101AudioProcessor::getStateInformation(juce::MemoryBlock& destData) {
    auto state = apvts_.copyState();
    // The active preset is stored alongside the controls so reopening a session
    // restores the sequence memory for the sequenced presets.
    state.setProperty("preset", currentPreset_.load(), nullptr);
    std::unique_ptr<juce::XmlElement> xml(state.createXml());
    if (xml != nullptr) copyXmlToBinary(*xml, destData);
}

void SH101AudioProcessor::setStateInformation(const void* data, int sizeInBytes) {
    std::unique_ptr<juce::XmlElement> xml(getXmlFromBinary(data, sizeInBytes));
    if (xml == nullptr) return;

    const int presetIndex = xml->getIntAttribute("preset", 0);
    // replaceState restores the control values; re-queueing the preset's pattern
    // restores the sequence memory that the parameters cannot carry.
    apvts_.replaceState(juce::ValueTree::fromXml(*xml));
    if (sh101::preset(presetIndex).applySequence != nullptr) {
        pendingPatternIndex_.store(presetIndex + 1, std::memory_order_release);
    }
    currentPreset_.store(juce::jlimit(0, sh101::numPresets() - 1, presetIndex),
                         std::memory_order_relaxed);
}

juce::AudioProcessor* JUCE_CALLTYPE createPluginFilter() {
    return new SH101AudioProcessor();
}
