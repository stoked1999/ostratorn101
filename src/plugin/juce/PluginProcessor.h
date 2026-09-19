// ÖstraTorn101 — VST3/Standalone wrapper for the SH-101 voice model.
//
// Design notes:
//   * The processor owns a sh101::SH101HostAdapter and does no DSP of its own.
//   * Every control is exposed as a host parameter.  Continuous controls are
//     normalized 0..1 floats; the switch-like controls are choice parameters, so
//     the host and the panel show real position names instead of anonymous
//     numbers.  Both carry the value the engine's taper expects (Params.h).
//   * The factory preset bank (src/sh101/Presets.h) is the plugin's program list,
//     so a host's own preset menu and the panel stay in step.
//   * User presets are files (src/sh101/PresetIO.h), saved under the user's
//     application data.  Saving/loading them is a message-thread operation; the
//     only thing that crosses to the audio thread is the sequencer pattern, which
//     is handed over under a try-lock so the callback never blocks.
#pragma once

#include <juce_audio_processors/juce_audio_processors.h>

#include <array>
#include <atomic>
#include <vector>

#include "plugin/SH101HostAdapter.h"
#include "sh101/PresetIO.h"

class OstraTornAudioProcessor : public juce::AudioProcessor {
public:
    OstraTornAudioProcessor();
    ~OstraTornAudioProcessor() override = default;

    void prepareToPlay(double sampleRate, int samplesPerBlock) override;
    void releaseResources() override;
    bool isBusesLayoutSupported(const BusesLayout& layouts) const override;
    void processBlock(juce::AudioBuffer<float>& buffer, juce::MidiBuffer& midi) override;

    juce::AudioProcessorEditor* createEditor() override;
    bool hasEditor() const override { return true; }

    // The name a host shows.  JucePlugin_DisplayName is written as a C escape
    // sequence in the build (see src/plugin/juce/CMakeLists.txt), so the bytes are
    // the correct UTF-8 ones no matter what the toolchain does with the command
    // line; fromUTF8 decodes them into the string JUCE will use.
    const juce::String getName() const override {
        return juce::String::fromUTF8(JucePlugin_DisplayName);
    }
    bool acceptsMidi() const override { return true; }
    bool producesMidi() const override { return false; }
    bool isMidiEffect() const override { return false; }
    double getTailLengthSeconds() const override { return 0.5; }

    // The factory bank is the plugin's program list.
    int getNumPrograms() override;
    int getCurrentProgram() override;
    void setCurrentProgram(int index) override;
    const juce::String getProgramName(int index) override;
    void changeProgramName(int, const juce::String&) override {}

    void getStateInformation(juce::MemoryBlock& destData) override;
    void setStateInformation(const void* data, int sizeInBytes) override;

    // ---- Factory presets --------------------------------------------------
    void loadPreset(int index);
    int currentPresetIndex() const { return currentPreset_.load(std::memory_order_relaxed); }

    // ---- User preset library ----------------------------------------------
    // The library is one text file per preset under
    // <userApplicationData>/ÖstraTorn101/Presets.
    static juce::File userPresetDirectory();
    juce::StringArray userPresetNames() const;
    juce::File userPresetFile(const juce::String& name) const;
    bool saveUserPreset(const juce::String& name);      // captures what is playing
    bool loadUserPreset(const juce::String& name);
    bool loadUserPresetFile(const juce::File& file);
    bool deleteUserPreset(const juce::String& name);
    bool captureDocument(sh101::PresetDocument& out) const;   // current panel + sequence

    // Identity of what is loaded, for the display.
    juce::String currentPresetName() const;
    bool currentPresetIsUser() const { return currentPresetIsUser_; }
    bool presetIsModified() const;

    // ---- Panel state ------------------------------------------------------
    // Peak of the last rendered block, for the panel's level meter.
    float outputLevel() const { return outputLevel_.load(std::memory_order_relaxed); }
    // Standby mutes the output while the voice keeps running (no click on return).
    void setStandby(bool on);
    bool isStandby() const { return standby_.load(std::memory_order_relaxed); }

    // Exposed for the editor.
    juce::AudioProcessorValueTreeState& parameters() { return apvts_; }
    sh101::SH101HostAdapter& adapter() { return adapter_; }

    static const std::vector<juce::String>& parameterIds();
    static juce::String parameterId(int paramId);
    static juce::String parameterName(int paramId);

private:
    juce::AudioProcessorValueTreeState::ParameterLayout createParameterLayout();
    void applyDocument(const sh101::PresetDocument& doc, const juce::String& name, bool isUser);
    void queuePattern(const sh101::PresetDocument& doc);
    void applyPendingPattern();
    // Normalized snapshot of every parameter (see rawToNormalized_).
    void readParameterSnapshot(float* destination) const;

    sh101::SH101HostAdapter adapter_{};
    juce::AudioProcessorValueTreeState apvts_;
    std::array<std::atomic<float>*, sh101::kNumParams> parameterPointers_{};
    // JUCE's raw parameter value is the parameter's value in *its own* range: for
    // a choice parameter that is the item index (0..n-1), not the normalized
    // position the engine's taper expects.  This converts raw -> normalized in the
    // audio callback without a lock or an allocation.
    std::array<float, sh101::kNumParams> rawToNormalized_{};

    // Sequencer pattern hand-over.  The message thread fills `pendingPattern_`
    // under the lock; the audio thread only ever *tries* the lock, so it can
    // never be blocked by the UI.
    sh101::PresetDocument pendingPattern_{};
    std::atomic<bool> pendingPatternReady_{ false };
    juce::SpinLock patternLock_;

    std::atomic<float> outputLevel_{ 0.0f };
    std::atomic<bool> standby_{ false };
    std::atomic<int> currentPreset_{ 0 };

    juce::String currentPresetName_;
    bool currentPresetIsUser_ = false;
    // Snapshot of what the loaded preset set, so "modified" can be answered by
    // comparison instead of by listening to every parameter change.
    sh101::SH101Params loadedParams_{};

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(OstraTornAudioProcessor)
};
