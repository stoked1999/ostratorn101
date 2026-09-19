// JUCE editor for the SH-101 model — see PluginProcessor.h for the unverified
// status.
//
// The brief says to add a UI only after the DSP tests pass, and to keep the UI
// one-to-one with the original front panel.  This editor is deliberately the
// smallest honest option for now: it lays out one labelled slider per SH-101
// parameter (grouped like the panel: LFO, VCO, mixer, filter, envelope, VCA,
// performance, arpeggiator/sequencer) and nothing else — no branding, no extra
// features.  A panel-faithful custom UI is a later, purely cosmetic step.
#pragma once

#include <juce_audio_processors/juce_audio_processors.h>

#include <memory>
#include <vector>

#include "PluginProcessor.h"

class SH101AudioProcessorEditor : public juce::AudioProcessorEditor {
public:
    explicit SH101AudioProcessorEditor(SH101AudioProcessor& processor);
    ~SH101AudioProcessorEditor() override = default;

    void paint(juce::Graphics& g) override;
    void resized() override;

private:
    struct Control {
        std::unique_ptr<juce::Slider> slider;
        std::unique_ptr<juce::Label> label;
        std::unique_ptr<juce::AudioProcessorValueTreeState::SliderAttachment> attachment;
    };

    void addSection(const juce::String& title, const std::vector<int>& paramIds);

    SH101AudioProcessor& processor_;
    std::vector<std::unique_ptr<juce::Label>> sectionLabels_;
    std::vector<Control> controls_;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(SH101AudioProcessorEditor)
};
