#include "PluginEditor.h"

#include <array>
#include <cmath>

#include "sh101/Params.h"
#include "sh101/Presets.h"

using namespace sh101;
using namespace sh101ui;

namespace {

// ---- Layout ----------------------------------------------------------------
constexpr int kWindowWidth = 908;
constexpr int kWindowHeight = 576;
constexpr int kTopBarHeight = 92;
constexpr int kMargin = 12;
constexpr int kColumnGap = 12;
constexpr int kPanelHeight = 108;
constexpr int kPanelGap = 9;
constexpr int kPanelHeaderHeight = 20;
constexpr int kPanelPadding = 11;
constexpr int kControlBodyHeight = 56;
constexpr int kNameLabelHeight = 14;

// ---- Control names, in sh101::ParamId order --------------------------------
// Panel-style short names: the value read-out underneath already carries the
// number, so the label only has to say what the control is.
const char* const kDisplayName[kNumParams] = {
    "RATE",      "WAVE",      "RANGE",    "TUNE",     "LFO MOD",  "PW",
    "PWM SRC",   "PWM AMT",   "SAW",      "PULSE",    "SUB",      "SUB MODE",
    "NOISE",     "CUTOFF",    "RESONANCE","ENV AMT",  "LFO AMT",  "KYBD",
    "ATTACK",    "DECAY",     "SUSTAIN",  "RELEASE",  "TRIGGER",  "VCA",
    "TIME",      "MODE",      "VOLUME",   "BENDER",   "ARP ON",   "ARP MODE",
    "ARP RATE",  "ARP OCT",   "SEQ ON",   "SEQ RATE",
};

// ---- Tooltips, in sh101::ParamId order -------------------------------------
const char* const kTooltip[kNumParams] = {
    "LFO speed",
    "LFO waveform: triangle, square, random or noise",
    "Octave range: 16', 8', 4', 2'",
    "Fine tuning, +/-50 cents",
    "How much LFO reaches the pitch",
    "Pulse width: narrow counter-clockwise, square at the top",
    "Pulse-width modulation source: envelope, manual, LFO",
    "How much the PWM source moves the pulse width",
    "Sawtooth level in the source mixer",
    "Pulse level in the source mixer",
    "Sub-oscillator level in the source mixer",
    "Sub-oscillator mode: one octave down, two octaves down, or narrow two octaves",
    "Noise level in the source mixer",
    "Filter cutoff",
    "Filter resonance; at the top the filter self-oscillates",
    "Envelope amount into the filter, in octaves",
    "LFO amount into the filter, in octaves",
    "Keyboard tracking of the filter",
    "Envelope attack time",
    "Envelope decay time",
    "Envelope sustain level",
    "Envelope release time",
    "Envelope trigger: gate + trigger, gate only, or LFO",
    "VCA control: envelope or gate",
    "Portamento glide time",
    "Portamento: off, on, or auto (legato only)",
    "Output volume",
    "Bender pitch range in semitones (host pitch bend)",
    "Arpeggiator on/off",
    "Arpeggiator direction: up, down, up/down",
    "Arpeggiator clock rate",
    "Arpeggiator octave range",
    "Step sequencer on/off (runs while a key is held)",
    "Step sequencer clock rate",
};

juce::String millisecondsText(double seconds) {
    if (seconds < 1.0) return juce::String(juce::roundToInt(seconds * 1000.0)) + "ms";
    return juce::String(seconds, 2) + "s";
}

juce::String percentText(double value) {
    return juce::String(juce::roundToInt(value * 100.0)) + "%";
}

// Engineering read-out: the same taper the engine applies, so the panel shows
// the value the instrument is actually using.
juce::String formatParamValue(int paramId, double normalized) {
    SH101Params p;
    applyNormalized(p, paramId, normalized);

    switch (paramId) {
        case pLfoRate:        return juce::String(p.lfoRate, 2) + "Hz";
        case pVcoTune:        return juce::String(juce::roundToInt(p.vcoTune)) + "ct";
        case pVcoModDepth:    return percentText(p.vcoModDep);
        case pPulseWidth:     return percentText(p.pulseWidth);
        case pPwmAmount:      return percentText(p.pwmAmount);
        case pSawLevel:       return percentText(p.sawLevel);
        case pPulseLevel:     return percentText(p.pulseLevel);
        case pSubLevel:       return percentText(p.subLevel);
        case pNoiseLevel:     return percentText(p.noiseLevel);
        case pResonance:      return percentText(p.resonance);
        case pKeyTrack:       return percentText(p.keyTrack);
        case pSustain:        return percentText(p.sustain);
        case pVolume:         return percentText(p.volume);
        case pCutoff:
            return p.cutoff >= 1000.0 ? juce::String(p.cutoff / 1000.0, 2) + "kHz"
                                      : juce::String(juce::roundToInt(p.cutoff)) + "Hz";
        case pFilterEnvAmount: return juce::String(p.filterEnvAmt, 1) + "oct";
        case pFilterModAmount: return juce::String(p.filterModAmt, 1) + "oct";
        case pAttack:          return millisecondsText(p.attack);
        case pDecay:           return millisecondsText(p.decay);
        case pRelease:         return millisecondsText(p.release);
        case pPortamentoTime:  return millisecondsText(p.portamentoTime);
        case pBend:            return juce::String(p.bendSemitones, 1) + "st";
        case pArpRate:         return juce::String(p.arpRate, 1) + "Hz";
        case pSeqRate:         return juce::String(p.seqRate, 1) + "Hz";
        case pArpOctaves:      return juce::String(p.arpOctaves) + "oct";
        default:               return juce::String(normalized, 2);
    }
}

} // namespace

SH101AudioProcessorEditor::SH101AudioProcessorEditor(SH101AudioProcessor& processor)
    : juce::AudioProcessorEditor(&processor), processor_(processor) {
    setLookAndFeel(&lookAndFeel_);

    // ---- Title block -------------------------------------------------------
    titleLabel_.setText("SH-101", juce::dontSendNotification);
    titleLabel_.setFont(panelFontBold(25.0f));
    titleLabel_.setColour(juce::Label::textColourId, Palette::text);
    titleLabel_.setJustificationType(juce::Justification::centredLeft);
    addAndMakeVisible(titleLabel_);

    subtitleLabel_.setText("Analogue mono synth - emulation", juce::dontSendNotification);
    subtitleLabel_.setFont(panelFont(11.0f));
    subtitleLabel_.setColour(juce::Label::textColourId, Palette::textDim);
    subtitleLabel_.setJustificationType(juce::Justification::centredLeft);
    addAndMakeVisible(subtitleLabel_);

    // ---- Preset bar --------------------------------------------------------
    presetCaptionLabel_.setText("PRESET", juce::dontSendNotification);
    presetCaptionLabel_.setFont(panelFontBold(10.5f).withExtraKerningFactor(0.18f));
    presetCaptionLabel_.setColour(juce::Label::textColourId, Palette::textDim);
    presetCaptionLabel_.setJustificationType(juce::Justification::centredLeft);
    addAndMakeVisible(presetCaptionLabel_);

    for (int i = 0; i < numPresets(); ++i) {
        presetBox_.addItem(juce::String(preset(i).category) + ": " + preset(i).name, i + 1);
    }
    presetBox_.setTooltip("Preset bank (also exposed to the host as this plugin's programs)");
    presetBox_.onChange = [this] { loadPresetFromUi(presetBox_.getSelectedId() - 1); };
    addAndMakeVisible(presetBox_);

    previousButton_.setTooltip("Previous preset");
    previousButton_.onClick = [this] { stepPreset(-1); };
    addAndMakeVisible(previousButton_);

    nextButton_.setTooltip("Next preset");
    nextButton_.onClick = [this] { stepPreset(1); };
    addAndMakeVisible(nextButton_);

    presetDescriptionLabel_.setFont(panelFont(11.0f));
    presetDescriptionLabel_.setColour(juce::Label::textColourId, Palette::textDim);
    presetDescriptionLabel_.setJustificationType(juce::Justification::centredLeft);
    addAndMakeVisible(presetDescriptionLabel_);

    // ---- Controls ----------------------------------------------------------
    // Two columns in signal order, the way the panel is read left to right.
    addPanel(0, "LFO", { pLfoRate, pLfoWave });
    addPanel(0, "VCO", { pVcoRange, pVcoTune, pVcoModDepth, pPulseWidth, pPwmSource, pPwmAmount });
    addPanel(0, "SOURCE MIXER", { pSawLevel, pPulseLevel, pSubLevel, pSubMode, pNoiseLevel });
    addPanel(0, "VCF", { pCutoff, pResonance, pFilterEnvAmount, pFilterModAmount, pKeyTrack });

    addPanel(1, "ENV", { pAttack, pDecay, pSustain, pRelease, pEnvTrigger });
    addPanel(1, "VCA / PERFORMANCE", { pVcaMode, pPortamentoTime, pPortamentoMode, pBend, pVolume });
    addPanel(1, "ARPEGGIATOR / SEQUENCER",
             { pArpOn, pArpMode, pArpRate, pArpOctaves, pSeqOn, pSeqRate });

    refreshPresetDisplay();
    startTimerHz(8);

    // Sizing last, and only after the controls exist: resized() lays the
    // controls out, so calling setSize() before they are added leaves every
    // control at zero size and the window blank.
    setSize(kWindowWidth, kWindowHeight);
}

SH101AudioProcessorEditor::~SH101AudioProcessorEditor() {
    stopTimer();
    setLookAndFeel(nullptr);
}

void SH101AudioProcessorEditor::addPanel(int column, const juce::String& title,
                                        const std::vector<int>& paramIds) {
    Panel panel;
    panel.column = column;
    panel.title = title;
    panel.firstControl = static_cast<int>(controls_.size());
    panel.numControls = static_cast<int>(paramIds.size());
    panels_.push_back(panel);

    for (int id : paramIds) {
        Control control;
        control.paramId = id;
        const juce::String parameterId = SH101AudioProcessor::parameterId(id);
        const juce::String tooltip = juce::String(kTooltip[id]);

        int itemCount = 0;
        const char* const* items = paramChoiceItems(id, itemCount);
        control.isChoice = (items != nullptr && itemCount > 1);

        if (control.isChoice) {
            control.combo = std::make_unique<juce::ComboBox>();
            for (int k = 0; k < itemCount; ++k) control.combo->addItem(items[k], k + 1);
            control.combo->setTooltip(tooltip);
            addAndMakeVisible(*control.combo);
            control.comboAttachment =
                std::make_unique<juce::AudioProcessorValueTreeState::ComboBoxAttachment>(
                    processor_.parameters(), parameterId, *control.combo);
        } else {
            control.slider = std::make_unique<juce::Slider>(juce::Slider::LinearVertical,
                                                            juce::Slider::NoTextBox);
            control.slider->setRange(0.0, 1.0, 0.0);
            control.slider->setSliderSnapsToMousePosition(true);
            control.slider->setDoubleClickReturnValue(
                true, getNormalized(SH101Params{}, id));   // double-click = reference patch
            control.slider->setTooltip(tooltip);
            addAndMakeVisible(*control.slider);
            control.sliderAttachment =
                std::make_unique<juce::AudioProcessorValueTreeState::SliderAttachment>(
                    processor_.parameters(), parameterId, *control.slider);
            // After the attachment, not before: a SliderAttachment installs its
            // own value->text formatter (the raw parameter value), which would
            // replace ours.  The panel shows engineering units, so ours goes on
            // last.
            control.slider->textFromValueFunction = [id](double v) { return formatParamValue(id, v); };
        }

        control.nameLabel = std::make_unique<juce::Label>();
        control.nameLabel->setText(juce::String(kDisplayName[id]), juce::dontSendNotification);
        control.nameLabel->setJustificationType(juce::Justification::centred);
        control.nameLabel->setColour(juce::Label::textColourId, Palette::text);
        control.nameLabel->setInterceptsMouseClicks(false, false);
        addAndMakeVisible(*control.nameLabel);

        controls_.push_back(std::move(control));
    }
}

void SH101AudioProcessorEditor::layoutPanel(Panel& panel, juce::Rectangle<int> bounds) {
    auto inner = bounds.reduced(kPanelPadding);
    inner.removeFromTop(kPanelHeaderHeight + 4);

    const int count = panel.numControls;
    if (count <= 0) return;
    const int cellWidth = inner.getWidth() / count;

    for (int i = 0; i < count; ++i) {
        Control& control = controls_[static_cast<size_t>(panel.firstControl + i)];
        auto cell = inner.removeFromLeft(cellWidth).reduced(3, 0);
        auto body = cell.removeFromTop(kControlBodyHeight);

        if (control.isChoice) {
            const int comboHeight = 22;
            control.combo->setBounds(body.reduced(3, (body.getHeight() - comboHeight) / 2));
        } else {
            control.slider->setBounds(body);
            // The value read-out is drawn by the look-and-feel, so the slider
            // itself carries no text box.
            control.slider->setTextBoxStyle(juce::Slider::NoTextBox, false, 0, 0);
        }
        control.nameLabel->setBounds(cell.removeFromTop(kNameLabelHeight));
    }
}

void SH101AudioProcessorEditor::resized() {
    auto area = getLocalBounds();

    // ---- Top bar -----------------------------------------------------------
    auto topBar = area.removeFromTop(kTopBarHeight).reduced(kMargin, 0);
    auto titleRow = topBar.removeFromTop(46).withTrimmedTop(8);
    titleLabel_.setBounds(titleRow.removeFromLeft(190));
    subtitleLabel_.setBounds(titleRow.removeFromTop(20).withTrimmedLeft(0));

    auto presetRow = topBar.removeFromTop(30);
    presetCaptionLabel_.setBounds(presetRow.removeFromLeft(58));
    previousButton_.setBounds(presetRow.removeFromLeft(26).reduced(0, 3));
    presetRow.removeFromLeft(4);
    nextButton_.setBounds(presetRow.removeFromLeft(26).reduced(0, 3));
    presetRow.removeFromLeft(8);
    presetBox_.setBounds(presetRow.removeFromLeft(300).reduced(0, 4));
    presetRow.removeFromLeft(12);
    presetDescriptionLabel_.setBounds(presetRow);

    // ---- Panels ------------------------------------------------------------
    auto content = area.reduced(kMargin);
    const int columnWidth = (content.getWidth() - kColumnGap) / 2;

    int columnY[2] = { content.getY(), content.getY() };
    for (Panel& panel : panels_) {
        const int column = juce::jlimit(0, 1, panel.column);
        const int x = content.getX() + column * (columnWidth + kColumnGap);
        panel.bounds = juce::Rectangle<int>(x, columnY[column], columnWidth, kPanelHeight);
        columnY[column] += kPanelHeight + kPanelGap;

        layoutPanel(panel, panel.bounds);
    }
}

void SH101AudioProcessorEditor::paint(juce::Graphics& g) {
    // Window background: a shallow vertical gradient keeps the flat panel from
    // looking like an unrendered window.
    g.setGradientFill(juce::ColourGradient(Palette::windowBg.brighter(0.055f), 0.0f, 0.0f,
                                           Palette::windowBg.darker(0.30f), 0.0f,
                                           static_cast<float>(getHeight()), false));
    g.fillAll();

    // Top bar: a slightly lighter strip, closed by an accent hairline.
    auto topBar = getLocalBounds().removeFromTop(kTopBarHeight);
    g.setColour(Palette::panelBg.withAlpha(0.5f));
    g.fillRect(topBar);
    g.setColour(Palette::accent.withAlpha(0.45f));
    g.fillRect(topBar.getX(), topBar.getBottom() - 1, topBar.getWidth(), 1);

    // Section panels.
    for (const Panel& panel : panels_) {
        const juce::Rectangle<float> r = panel.bounds.toFloat();
        g.setColour(Palette::panelBg);
        g.fillRoundedRectangle(r, 6.0f);
        g.setColour(Palette::panelOutline);
        g.drawRoundedRectangle(r.reduced(0.5f), 6.0f, 1.0f);

        auto header = panel.bounds.reduced(kPanelPadding).removeFromTop(kPanelHeaderHeight);
        g.setColour(Palette::headerText);
        g.setFont(panelFontBold(12.0f).withExtraKerningFactor(0.14f));
        g.drawText(panel.title, header, juce::Justification::centredLeft, false);

        g.setColour(Palette::accent.withAlpha(0.40f));
        g.fillRect(header.getX(), header.getBottom() - 3, juce::jmin(header.getWidth(), 46), 2);
    }
}

// ---- Presets ---------------------------------------------------------------
void SH101AudioProcessorEditor::loadPresetFromUi(int index) {
    processor_.loadPreset(index);
    refreshPresetDisplay();
}

void SH101AudioProcessorEditor::stepPreset(int delta) {
    const int count = numPresets();
    if (count <= 0) return;
    const int current = processor_.currentPresetIndex();
    int next = current + delta;
    if (next < 0) next = count - 1;
    if (next >= count) next = 0;
    loadPresetFromUi(next);
}

void SH101AudioProcessorEditor::refreshPresetDisplay() {
    const int index = juce::jlimit(0, numPresets() - 1, processor_.currentPresetIndex());
    // dontSendNotification: this is a display refresh, not a user action.
    presetBox_.setSelectedId(index + 1, juce::dontSendNotification);
    presetDescriptionLabel_.setText(juce::String(preset(index).description),
                                    juce::dontSendNotification);
}

void SH101AudioProcessorEditor::timerCallback() {
    // The host can change programs behind our back (automation, preset menu),
    // so the display follows the processor rather than the other way round.
    if (presetBox_.getSelectedId() - 1 != processor_.currentPresetIndex()) {
        refreshPresetDisplay();
    }
}
