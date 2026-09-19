// JUCE editor for the SH-101 model.
//
// Layout policy: the panel is the instrument's own vocabulary — the sections are
// the synth's functional blocks in signal order (LFO, VCO, source mixer,
// filter, envelope, VCA/performance, arpeggiator/sequencer), each control on a
// fader slot with the value it currently holds underneath it.  Switch-like
// controls are drop-downs showing the real position names.
//
// Values are shown in engineering units (Hz, ms, %, octaves) computed through
// the same taper the engine uses, not as raw 0..1 numbers.
//
// The preset selector drives the shared preset bank (src/sh101/Presets.h), which
// is also the plugin's program list, so the host's own preset menu and this
// panel stay in step.
#pragma once

#include <juce_audio_processors/juce_audio_processors.h>
#include <juce_gui_extra/juce_gui_extra.h>

#include <memory>
#include <vector>

#include "PluginProcessor.h"
#include "SH101LookAndFeel.h"

class SH101AudioProcessorEditor : public juce::AudioProcessorEditor,
                                 private juce::Timer {
public:
    explicit SH101AudioProcessorEditor(SH101AudioProcessor& processor);
    ~SH101AudioProcessorEditor() override;

    void paint(juce::Graphics& g) override;
    void resized() override;

private:
    struct Control {
        int paramId = -1;
        bool isChoice = false;
        std::unique_ptr<juce::Slider> slider;
        std::unique_ptr<juce::ComboBox> combo;
        std::unique_ptr<juce::Label> nameLabel;
        std::unique_ptr<juce::AudioProcessorValueTreeState::SliderAttachment> sliderAttachment;
        std::unique_ptr<juce::AudioProcessorValueTreeState::ComboBoxAttachment> comboAttachment;
    };

    struct Panel {
        int column = 0;             // 0 = left, 1 = right
        juce::String title;
        int firstControl = 0;
        int numControls = 0;
        juce::Rectangle<int> bounds;
    };

    void addPanel(int column, const juce::String& title, const std::vector<int>& paramIds);
    void layoutPanel(Panel& panel, juce::Rectangle<int> bounds);
    void loadPresetFromUi(int index);
    void stepPreset(int delta);
    void refreshPresetDisplay();
    void timerCallback() override;

    SH101AudioProcessor& processor_;
    SH101LookAndFeel lookAndFeel_;

    std::vector<Control> controls_;
    std::vector<Panel> panels_;

    juce::Label titleLabel_;
    juce::Label subtitleLabel_;
    juce::Label presetCaptionLabel_;
    juce::Label presetDescriptionLabel_;
    juce::ComboBox presetBox_;
    juce::TextButton previousButton_{ "<" };
    juce::TextButton nextButton_{ ">" };
    juce::TooltipWindow tooltipWindow_{ this, 600 };

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(SH101AudioProcessorEditor)
};
