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
//   * The sequencer pattern is edited on the message thread against a mirror of
//     the sequence memory (patternMirror_); the audio thread owns the engine's
//     sequencer and receives published copies.  Keyboard notes for REC travel the
//     other way through a lock-free queue, so neither thread ever waits.
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

    // ---- Sequencer pattern (the step editor) -------------------------------
    // The panel edits the message thread's mirror of the sequence memory; the
    // length is the seqLength host parameter.  Every edit is published to the
    // audio thread through the lock-free pattern hand-over below, so the panel
    // and the callback never share a lock.  The mirror starts out as the engine's
    // own default does: note 60 on every step.
    const sh101::StepSequencer::Step& patternStep(int index) const;
    int  patternLength() const;                 // from the seqLength parameter
    void setPatternStep(int index, int note, bool gate, bool tie);
    void setPatternLength(int steps);           // updates the parameter
    void publishPattern();                      // mirror -> audio thread

    // Playback telemetry for the playhead, published by the audio thread.
    int  sequencePlayStep() const { return seqPlayStep_.load(std::memory_order_relaxed); }
    bool sequenceRunning() const { return seqRunning_.load(std::memory_order_relaxed); }
    bool sequenceGateHigh() const { return seqGateHigh_.load(std::memory_order_relaxed); }

    // ---- Signal feed for the cymatic display ------------------------------
    // The audio thread appends the block it just rendered to a lock-free ring;
    // the display reads the most recent samples on the message thread.  Bounded
    // and allocation-free: the callback only copies what it has already written.
    int readScopeSamples(float* destination, int maxSamples) const;

    // ---- Keyboard capture (REC) -------------------------------------------
    // Notes the host delivered, stamped with the step the sequencer was playing
    // when they arrived.  The audio thread appends, the message thread drains
    // (a lock-free SPSC queue).
    struct CapturedNote {
        int  note = -1;    // MIDI note number
        bool on = false;   // note on / note off
        int  step = -1;    // sequence step at that moment; -1 while stopped
    };
    bool takeCapturedNote(CapturedNote& out);

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
    // Message thread: make the mirror and the audio thread's copy match a
    // document's pattern.
    void adoptPattern(const sh101::PresetDocument& doc);
    void queuePattern(const sh101::PresetDocument& doc);
    void applyPendingPattern();
    // Normalized snapshot of every parameter (see rawToNormalized_).
    void readParameterSnapshot(float* destination) const;
    float normalizedParam(int paramId) const;
    // Audio thread: append one keyboard event to the capture queue.
    void captureNote(int note, bool on, int step);
    // Audio thread: append the rendered block to the display's ring.
    void writeScope(const float* samples, int numSamples);

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
    // The message thread's copy of the sequence memory (what the step editor
    // edits, saves and restores), and the copy from the last load, so "modified"
    // notices a pattern edit as well as a control edit.
    std::array<sh101::StepSequencer::Step, sh101::StepSequencer::kMaxSteps> patternMirror_{};
    std::array<sh101::StepSequencer::Step, sh101::StepSequencer::kMaxSteps> patternAtLoad_{};

    // Playback telemetry for the playhead.
    std::atomic<int>  seqPlayStep_{ -1 };
    std::atomic<bool> seqRunning_{ false };
    std::atomic<bool> seqGateHigh_{ false };

    // Keyboard capture for REC (SPSC, audio thread -> message thread).
    static constexpr int kCaptureQueueSize = 128;
    std::array<CapturedNote, kCaptureQueueSize> captureQueue_{};
    std::atomic<unsigned int> captureWrite_{ 0 };
    std::atomic<unsigned int> captureRead_{ 0 };

    // Signal feed for the cymatic display (SPSC ring, audio -> message thread).
    // A power of two, so the index wrap is a mask.
    static constexpr int kScopeRingSize = 8192;
    std::array<float, kScopeRingSize> scopeRing_{};
    std::atomic<unsigned int> scopeWrite_{ 0 };

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
