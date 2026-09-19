// JUCE editor for the SH-101 model — see PluginProcessor.h for the unverified
// status.
//
// The brief says to add a UI only after the DSP tests pass, and to keep the UI
// one-to-one with the original front panel.  This editor is deliberately the
// smallest honest option for now: one labelled control per SH-101 parameter,
// grouped like the panel (LFO, VCO, source mixer, VCF, ENV, VCA/performance,
// arpeggiator/sequencer) and nothing else — no branding, no extra features.  A
// panel-faithful custom UI is a later, purely cosmetic step.
#include "PluginEditor.h"

#include "sh101/Params.h"

using namespace sh101;

namespace {
constexpr int kSliderWidth = 84;
constexpr int kSliderHeight = 96;
constexpr int kSectionHeaderHeight = 22;
constexpr int kMargin = 10;
constexpr int kColumns = 6;
constexpr int kLabelHeight = 16;
constexpr int kCellPadding = 8;

// Section sizes must match the groups added in the constructor.
const int kSectionSizes[] = { 2, 6, 5, 5, 5, 5, 6 };
constexpr int kNumSections = static_cast<int>(sizeof(kSectionSizes) / sizeof(kSectionSizes[0]));
} // namespace

SH101AudioProcessorEditor::SH101AudioProcessorEditor(SH101AudioProcessor& processor)
    : juce::AudioProcessorEditor(&processor), processor_(processor) {
    setSize(600, 700);

    addSection("LFO", { pLfoRate, pLfoWave });
    addSection("VCO", { pVcoRange, pVcoTune, pVcoModDepth, pPulseWidth, pPwmSource, pPwmAmount });
    addSection("SOURCE MIXER", { pSawLevel, pPulseLevel, pSubLevel, pSubMode, pNoiseLevel });
    addSection("VCF", { pCutoff, pResonance, pFilterEnvAmount, pFilterModAmount, pKeyTrack });
    addSection("ENV", { pAttack, pDecay, pSustain, pRelease, pEnvTrigger });
    addSection("VCA / PERFORMANCE", { pVcaMode, pPortamentoTime, pPortamentoMode, pBend, pVolume });
    addSection("ARPEGGIATOR / SEQUENCER",
               { pArpOn, pArpMode, pArpRate, pArpOctaves, pSeqOn, pSeqRate });
}

void SH101AudioProcessorEditor::addSection(const juce::String& title,
                                           const std::vector<int>& paramIds) {
    auto sectionLabel = std::make_unique<juce::Label>();
    sectionLabel->setText(title, juce::dontSendNotification);
    sectionLabel->setJustificationType(juce::Justification::centredLeft);
    sectionLabel->setColour(juce::Label::textColourId, juce::Colours::white);
    addAndMakeVisible(*sectionLabel);
    sectionLabels_.push_back(std::move(sectionLabel));

    for (int id : paramIds) {
        Control control;
        const juce::String paramId = SH101AudioProcessor::parameterId(id);

        control.slider = std::make_unique<juce::Slider>(juce::Slider::RotaryHorizontalVerticalDrag,
                                                        juce::Slider::TextBoxBelow);
        control.slider->setRange(0.0, 1.0, 0.0);
        control.slider->setTooltip("Normalized 0..1; the original SH-101 taper is applied inside "
                                   "the engine (see src/sh101/Params.h).");
        addAndMakeVisible(*control.slider);

        control.label = std::make_unique<juce::Label>();
        control.label->setText(SH101AudioProcessor::parameterName(id), juce::dontSendNotification);
        control.label->setJustificationType(juce::Justification::centred);
        control.label->setColour(juce::Label::textColourId, juce::Colours::whitesmoke);
        addAndMakeVisible(*control.label);

        control.attachment =
            std::make_unique<juce::AudioProcessorValueTreeState::SliderAttachment>(
                processor_.parameters(), paramId, *control.slider);

        controls_.push_back(std::move(control));
    }
}

void SH101AudioProcessorEditor::paint(juce::Graphics& g) {
    g.fillAll(juce::Colour::fromRGB(26, 27, 31));
}

void SH101AudioProcessorEditor::resized() {
    auto area = getLocalBounds().reduced(kMargin);
    const int cellWidth = area.getWidth() / kColumns;

    int y = area.getY();
    int cursor = 0;
    for (int s = 0; s < kNumSections && s < static_cast<int>(sectionLabels_.size()); ++s) {
        sectionLabels_[static_cast<size_t>(s)]->setBounds(area.getX(), y, area.getWidth(),
                                                         kSectionHeaderHeight);
        y += kSectionHeaderHeight;

        const int count = kSectionSizes[s];
        for (int i = 0; i < count; ++i) {
            const int column = i % kColumns;
            const int row = i / kColumns;
            const int x = area.getX() + column * cellWidth;
            const int cellY = y + row * (kSliderHeight + kLabelHeight + kCellPadding);
            Control& c = controls_[static_cast<size_t>(cursor + i)];
            c.slider->setBounds(x, cellY, kSliderWidth, kSliderHeight);
            c.label->setBounds(x, cellY + kSliderHeight, kSliderWidth, kLabelHeight);
        }
        const int rows = (count + kColumns - 1) / kColumns;
        y += rows * (kSliderHeight + kLabelHeight + kCellPadding) + kCellPadding;
        cursor += count;
    }
}
