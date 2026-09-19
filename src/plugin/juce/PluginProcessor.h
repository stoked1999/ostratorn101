// JUCE VST3/Standalone wrapper for the SH-101 model.
//
// Design notes:
//   * The processor owns a sh101::SH101HostAdapter and does no DSP of its own.
//   * Every SH-101 control is exposed as a host parameter.  Continuous controls
//     are normalized 0..1 floats; the switch-like controls (range, sub mode,
//     PWM source, trigger mode, VCA mode, portamento mode, arp mode, arp/seq
//     on) are choice parameters so the host and the editor can show the real
//     position names instead of anonymous numbers.  Both kinds carry the same
//     normalized value the engine's taper expects (see src/sh101/Params.h), so
//     automation, the editor and the DSP agree.
//   * The preset bank (src/sh101/Presets.h) is exposed as the plugin's programs,
//     so a host shows the presets in its own preset list as well as in the
//     editor.
//   * Parameters are pushed to the adapter once per block, not once per
//     parameter, so a block performs a single parameter commit.
//
// Threading note: a preset may carry a sequencer pattern.  The pattern lives in
// the engine's sequence memory, not in the parameter tree, so it is handed to
// the audio thread through an atomic index and applied at the top of the next
// block.  The message thread never writes it, which keeps the sequence memory
// single-writer without locks or allocation in the callback.
#pragma once

#include <juce_audio_processors/juce_audio_processors.h>

#include <array>
#include <atomic>
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

    // The preset bank is the plugin's program list.
    int getNumPrograms() override;
    int getCurrentProgram() override;
    void setCurrentProgram(int index) override;
    const juce::String getProgramName(int index) override;
    void changeProgramName(int, const juce::String&) override {}

    void getStateInformation(juce::MemoryBlock& destData) override;
    void setStateInformation(const void* data, int sizeInBytes) override;

    // ---- Presets ----------------------------------------------------------
    // Loads every control from the preset bank and queues the preset's
    // sequencer pattern (if it has one) for the audio thread.
    void loadPreset(int index);
    int currentPresetIndex() const { return currentPreset_.load(std::memory_order_relaxed); }

    // Exposed for the editor.
    juce::AudioProcessorValueTreeState& parameters() { return apvts_; }
    sh101::SH101HostAdapter& adapter() { return adapter_; }

    // Parameter identifiers, one per sh101::ParamId, in the same order.
    static const std::vector<juce::String>& parameterIds();
    static juce::String parameterId(int paramId);
    static juce::String parameterName(int paramId);

private:
    juce::AudioProcessorValueTreeState::ParameterLayout createParameterLayout();

    void applyPendingPattern();

    sh101::SH101HostAdapter adapter_{};
    juce::AudioProcessorValueTreeState apvts_;
    std::array<std::atomic<float>*, sh101::kNumParams> parameterPointers_{};

    // 0 = nothing pending, otherwise (preset index + 1).
    std::atomic<int> pendingPatternIndex_{ 0 };
    std::atomic<int> currentPreset_{ 0 };

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(SH101AudioProcessor)
};
