// ÖstraTorn101 — panel editor.
//
// The panel is laid out like hardware: a title bar with the instrument's name and
// the power switch, a preset row with the display and the library buttons, then
// two rows of sections in signal order —
//
//   row 1:  LFO · VCO · SOURCE MIXER · VCF · VCA · ENV
//   row 2:  ARPEGGIATOR · SEQUENCER · PERFORMANCE ·  nameplate and level meter
//
// Each control is a fader with an amber cap and the value it currently holds
// underneath, in engineering units (Hz, ms, %, oct, ct); switch-like controls are
// drop-downs showing the real position names.
#pragma once

#include <juce_audio_processors/juce_audio_processors.h>
#include <juce_gui_extra/juce_gui_extra.h>

#include <memory>
#include <vector>

#include "CymaticDisplay.h"
#include "OstraTornLookAndFeel.h"
#include "PluginProcessor.h"
#include "StepEditor.h"

// The panel's power switch: a rocker that mutes the output (standby).
class RockerSwitch : public juce::Button {
public:
    explicit RockerSwitch(const juce::String& name);
    void paintButton(juce::Graphics& g, bool shouldDrawButtonAsHighlighted,
                     bool shouldDrawButtonAsDown) override;
};

class OstraTornAudioProcessorEditor : public juce::AudioProcessorEditor,
                                      private juce::Timer {
public:
    explicit OstraTornAudioProcessorEditor(OstraTornAudioProcessor& processor);
    ~OstraTornAudioProcessorEditor() override;

    void paint(juce::Graphics& g) override;
    void resized() override;

    // The step editor and the cymatic display, exposed so the editor test can
    // drive them without a running message loop.
    StepEditor& stepEditor() { return stepEditor_; }
    CymaticDisplay& cymaticDisplay() { return cymaticDisplay_; }

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
        int row = 0;                 // 0 = upper row, 1 = lower row
        int contentWidth = 0;        // sum of its controls' widths, in pixels
        juce::String title;
        int firstControl = 0;
        int numControls = 0;
        juce::Rectangle<int> bounds;
    };

    void addPanel(int row, const juce::String& title, const std::vector<int>& paramIds);
    void layoutPanel(Panel& panel, juce::Rectangle<int> bounds, float widthScale);
    void layoutRow(int row, juce::Rectangle<int> area, int rightHandBlockWidth);

    void refreshPresetList();
    void refreshPresetDisplay();
    void loadPresetFromUi(int combinedIndex);
    void stepPreset(int delta);
    int combinedPresetCount() const;
    void showPresetMenu();
    void savePresetFromUi();
    void loadPresetFromFileUi();
    void deleteUserPresetUi(const juce::String& name);
    void timerCallback() override;

    void paintTitleBar(juce::Graphics& g);
    void paintLevelMeter(juce::Graphics& g, juce::Rectangle<int> bounds);

    // The right-hand block of the lower row holds the nameplate and the meter;
    // both the painting and the label layout ask for these, so they cannot drift
    // apart.
    juce::Rectangle<int> lowerRightBlockBounds() const;
    juce::Rectangle<int> nameplateBounds() const;
    juce::Rectangle<int> cymaticBounds() const;
    juce::Rectangle<int> levelMeterBounds() const;
    void updatePresetDescription();

    OstraTornAudioProcessor& processor_;
    OstraTornLookAndFeel lookAndFeel_;
    StepEditor stepEditor_;
    CymaticDisplay cymaticDisplay_;

    std::vector<Control> controls_;
    std::vector<Panel> panels_;

    juce::Label titleLabel_;
    juce::Label subtitleLabel_;
    juce::Label presetCaptionLabel_;
    juce::Label descriptionLabel_;
    juce::Label nameplateTitle_;
    juce::Label nameplateSubtitle_;
    juce::Label inspiredLabel_;

    juce::ComboBox presetBox_;
    juce::TextButton previousButton_{ "<" };
    juce::TextButton nextButton_{ ">" };
    juce::TextButton saveButton_{ "SAVE" };
    juce::TextButton loadButton_{ "LOAD" };
    juce::TextButton menuButton_{ "MENU" };
    RockerSwitch powerButton_{ "POWER" };
    juce::TooltipWindow tooltipWindow_{ this, 600 };
    std::unique_ptr<juce::FileChooser> fileChooser_;

    // Level meter: the processor reports each block's peak, the panel smooths it
    // with a decay so the ladder reads like a real meter.
    float meterLevel_ = 0.0f;

    // Snapshot of the preset list, so the combo and the menu agree.
    juce::StringArray userPresets_;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(OstraTornAudioProcessorEditor)
};
