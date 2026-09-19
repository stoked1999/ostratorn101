// JUCE VST3/Standalone wrapper — see PluginProcessor.h for the unverified status.
#include "PluginProcessor.h"

#include "PluginEditor.h"
#include "sh101/Params.h"

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
    // Every SH-101 control is a normalized 0..1 float; Params.h applies the
    // original taper.  The defaults come from the engine's own default patch
    // (SH101Params), so a freshly loaded instance is immediately playable and
    // matches the reference patch used by the tests and the renderer.  Defaulting
    // every parameter to 0 would load a silent instrument: all four source levels
    // and the volume are parameters here.
    const sh101::SH101Params defaultPatch{};
    for (int i = 0; i < sh101::kNumParams; ++i) {
        const juce::String id = parameterId(i);
        const juce::String name = parameterName(i);
        const auto defaultValue = static_cast<float>(sh101::getNormalized(defaultPatch, i));
        layout.add(std::make_unique<juce::AudioParameterFloat>(
            juce::ParameterID{ id, 1 }, name, juce::NormalisableRange<float>(0.0f, 1.0f),
            defaultValue));
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
    adapter_.setAllParametersNormalized(0.5);   // neutral starting point
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

void SH101AudioProcessor::processBlock(juce::AudioBuffer<float>& buffer,
                                      juce::MidiBuffer& midi) {
    juce::ScopedNoDenormals noDenormals;
    const int numSamples = buffer.getNumSamples();

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

juce::AudioProcessorEditor* SH101AudioProcessor::createEditor() {
    return new SH101AudioProcessorEditor(*this);
}

void SH101AudioProcessor::getStateInformation(juce::MemoryBlock& destData) {
    const auto state = apvts_.copyState();
    std::unique_ptr<juce::XmlElement> xml(state.createXml());
    if (xml != nullptr) copyXmlToBinary(*xml, destData);
}

void SH101AudioProcessor::setStateInformation(const void* data, int sizeInBytes) {
    std::unique_ptr<juce::XmlElement> xml(getXmlFromBinary(data, sizeInBytes));
    if (xml != nullptr) {
        apvts_.replaceState(juce::ValueTree::fromXml(*xml));
    }
}

juce::AudioProcessor* JUCE_CALLTYPE createPluginFilter() {
    return new SH101AudioProcessor();
}
