// JUCE VST3/Standalone wrapper for the SH-101 model.
//
// STATUS: NOT COMPILED/VALIDATED IN THE DEVELOPMENT ENVIRONMENT.
// The machine this project was built on has no MSVC C++ toolchain (Visual Studio
// 2022 is installed without the "Desktop development with C++" workload) and no
// JUCE checkout, so the plugin shell could not be built there.  Everything it
// depends on *is* compiled and tested: the whole voice and the host-facing layer
// (sh101::SH101HostAdapter, covered by tests/test_host_adapter.cpp).  This file
// is therefore a conventional JUCE shell over verified code — treat it as
// unverified until it builds.  See docs/VST3.md for the exact build steps.
//
// Design notes:
//   * The processor owns a sh101::SH101HostAdapter and does no DSP of its own.
//   * All 34 SH-101 parameters are exposed as normalized 0..1
//     AudioParameterFloats; the taper to engineering units lives in Params.h, so
//     automation lanes, the editor and the DSP all agree on the mapping.
//   * Parameters are pushed to the adapter once per block, not once per
//     parameter, so a block performs a single parameter commit.
#pragma once

#include <juce_audio_processors/juce_audio_processors.h>

#include <array>
#include <vector>

#include "plugin/SH101HostAdapter.h"

class SH101AudioProcessor : public juce::AudioProcessor {
public:
    SH101AudioProcessor();
    ~SH101AudioProcessor() override = default;

    void prepareToPlay(double sampleRate, int samplesPerBlock) override;
    void releaseResources() override;
    bool isBusesLayoutSupported(const BusesLayout& layouts) const override;
    void processBlock(juce::AudioBuffer<float>& buffer, juce::MidiBuffer& midi) override;

    juce::AudioProcessorEditor* createEditor() override;
    bool hasEditor() const override { return true; }

    const juce::String getName() const override { return JucePlugin_Name; }
    bool acceptsMidi() const override { return true; }
    bool producesMidi() const override { return false; }
    bool isMidiEffect() const override { return false; }
    double getTailLengthSeconds() const override { return 0.5; }

    int getNumPrograms() override { return 1; }
    int getCurrentProgram() override { return 0; }
    void setCurrentProgram(int) override {}
    const juce::String getProgramName(int) override { return {}; }
    void changeProgramName(int, const juce::String&) override {}

    void getStateInformation(juce::MemoryBlock& destData) override;
    void setStateInformation(const void* data, int sizeInBytes) override;

    // Exposed for the editor.
    juce::AudioProcessorValueTreeState& parameters() { return apvts; }
    sh101::SH101HostAdapter& adapter() { return adapter_; }

    // Parameter identifiers, one per sh101::ParamId, in the same order.
    static const std::vector<juce::String>& parameterIds();
    static juce::String parameterId(int paramId);
    static juce::String parameterName(int paramId);

private:
    juce::AudioProcessorValueTreeState::ParameterLayout createParameterLayout();

    sh101::SH101HostAdapter adapter_{};
    juce::AudioProcessorValueTreeState apvts_;
    std::array<std::atomic<float>*, sh101::kNumParams> parameterPointers_{};

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(SH101AudioProcessor)
};
