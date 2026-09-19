#include "PluginEditor.h"

#include <cmath>

#include "sh101/Params.h"
#include "sh101/Presets.h"

using namespace ostra;
using namespace sh101;

namespace {

// ---- Layout -----------------------------------------------------------------
constexpr int kWindowWidth = 1440;
constexpr int kWindowHeight = 646;
constexpr int kMargin = 12;
constexpr int kTitleBarHeight = 104;
constexpr int kPresetBarHeight = 60;
constexpr int kRowHeight = 214;
constexpr int kRowGap = 10;
constexpr int kPanelHeaderHeight = 22;
constexpr int kPanelPadding = 9;
// A fader needs less width than a switch: a switch has to show a position name, so
// the two control types get different cells (and the panels are sized to match).
constexpr int kFaderCellWidth = 44;
constexpr int kSwitchCellWidth = 66;
constexpr int kMaxPanelGap = 56;    // panels never drift further apart than this
constexpr int kControlHeight = 156; // fader travel + cap
constexpr int kNameLabelHeight = 14;
constexpr int kLowerBlockWidth = 360;   // nameplate + level meter in the lower row

// ---- Panel short names, in sh101::ParamId order ------------------------------
// Panel lettering is short by nature (the hardware prints as little as it can);
// the full names live in the tooltips.
const char* const kDisplayName[kNumParams] = {
    "RATE",      "WAVE",      "RANGE",   "TUNE",    "LFO MOD",  "PW",
    "PWM SRC",   "PWM AMT",   "SAW",     "PULSE",   "SUB",      "SUB MODE",
    "NOISE",     "CUTOFF",    "RES",     "ENV AMT", "LFO AMT",  "KYBD",
    "ATK",       "DEC",       "SUS",     "REL",     "TRIG",     "VCA",
    "TIME",      "MODE",      "VOL",     "BEND",    "ARP ON",   "ARP MODE",
    "ARP RATE",  "ARP OCT",   "SEQ ON",  "SEQ RATE",
};

// ---- Tooltips, in sh101::ParamId order ---------------------------------------
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

// Engineering read-out, through the same taper the engine uses.
juce::String formatParamValue(int paramId, double normalized) {
    SH101Params p;
    applyNormalized(p, paramId, normalized);

    switch (paramId) {
        case pLfoRate:         return juce::String(p.lfoRate, 2) + "Hz";
        case pVcoTune:         return juce::String(juce::roundToInt(p.vcoTune)) + "ct";
        case pVcoModDepth:     return percentText(p.vcoModDep);
        case pPulseWidth:      return percentText(p.pulseWidth);
        case pPwmAmount:       return percentText(p.pwmAmount);
        case pSawLevel:        return percentText(p.sawLevel);
        case pPulseLevel:      return percentText(p.pulseLevel);
        case pSubLevel:        return percentText(p.subLevel);
        case pNoiseLevel:      return percentText(p.noiseLevel);
        case pResonance:       return percentText(p.resonance);
        case pKeyTrack:        return percentText(p.keyTrack);
        case pSustain:         return percentText(p.sustain);
        case pVolume:          return percentText(p.volume);
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

constexpr int kUserPresetIdBase = 10000;

} // namespace

// ---- Power switch -----------------------------------------------------------
RockerSwitch::RockerSwitch(const juce::String& name) : juce::Button(name) {
    setClickingTogglesState(true);
    setTooltip("Power: standby mutes the output and lets the voice keep running");
}

void RockerSwitch::paintButton(juce::Graphics& g, bool highlighted, bool down) {
    const auto r = getLocalBounds().toFloat().reduced(0.5f);
    const bool on = getToggleState();

    g.setColour(juce::Colour(0xff1b1b1e));
    g.fillRoundedRectangle(r, 4.0f);

    // Rocker: the "down" half is the lit face when the switch is on.
    juce::Rectangle<float> face = r.reduced(3.0f);
    const float half = face.getHeight() * 0.5f;
    juce::Rectangle<float> lit = on ? face.removeFromBottom(half) : face.removeFromTop(half);

    g.setGradientFill(juce::ColourGradient(juce::Colour(0xff4a4a4f), lit.getX(), lit.getY(),
                                           juce::Colour(0xff25252a), lit.getX(), lit.getBottom(), false));
    g.fillRoundedRectangle(lit, 2.5f);

    juce::Rectangle<float> other = on ? r.reduced(3.0f).removeFromTop(half)
                                      : r.reduced(3.0f).removeFromBottom(half);
    g.setColour(juce::Colour(0xff3a3a3e).brighter(highlighted || down ? 0.18f : 0.0f));
    g.fillRoundedRectangle(other, 2.5f);

    g.setColour(juce::Colours::black.withAlpha(0.6f));
    g.drawLine(r.getX() + 4.0f, r.getCentreY(), r.getRight() - 4.0f, r.getCentreY(), 1.0f);
    g.setColour(juce::Colours::white.withAlpha(0.10f));
    g.drawRoundedRectangle(r, 4.0f, 1.0f);
}

// ---- Editor -----------------------------------------------------------------
OstraTornAudioProcessorEditor::OstraTornAudioProcessorEditor(OstraTornAudioProcessor& processor)
    : juce::AudioProcessorEditor(&processor), processor_(processor) {
    setLookAndFeel(&lookAndFeel_);

    // ---- Title bar ---------------------------------------------------------
    // The product name is UTF-8 (built as a C escape sequence in CMake):
    // juce::String(const char*) would read it as Latin-1 and show mojibake.
    titleLabel_.setText(juce::String::fromUTF8(JucePlugin_DisplayName), juce::dontSendNotification);
    titleLabel_.setFont(boldFont(38.0f).withExtraKerningFactor(-0.02f));
    titleLabel_.setColour(juce::Label::textColourId, Palette::engrave);
    titleLabel_.setJustificationType(juce::Justification::centredLeft);
    addAndMakeVisible(titleLabel_);

    subtitleLabel_.setText("ANALOGUE MONO SYNTHESIZER", juce::dontSendNotification);
    subtitleLabel_.setFont(engravedFont(11.5f));
    subtitleLabel_.setColour(juce::Label::textColourId, Palette::engraveSoft);
    subtitleLabel_.setJustificationType(juce::Justification::centredLeft);
    addAndMakeVisible(subtitleLabel_);

    powerButton_.setToggleState(! processor_.isStandby(), juce::dontSendNotification);
    powerButton_.onClick = [this] {
        processor_.setStandby(! powerButton_.getToggleState());
        repaint();
    };
    addAndMakeVisible(powerButton_);

    // ---- Preset row --------------------------------------------------------
    presetCaptionLabel_.setText("PRESET", juce::dontSendNotification);
    presetCaptionLabel_.setFont(engravedFont(11.0f));
    // The preset strip is a dark tinted window, so its lettering is light.
    presetCaptionLabel_.setColour(juce::Label::textColourId, Palette::displayText.withAlpha(0.7f));
    presetCaptionLabel_.setJustificationType(juce::Justification::centredLeft);
    addAndMakeVisible(presetCaptionLabel_);

    presetBox_.setTooltip("The factory bank is the plugin's program list; user presets are files");
    presetBox_.onChange = [this] { loadPresetFromUi(presetBox_.getSelectedId() - 1); };
    addAndMakeVisible(presetBox_);

    previousButton_.setTooltip("Previous preset");
    previousButton_.onClick = [this] { stepPreset(-1); };
    addAndMakeVisible(previousButton_);

    nextButton_.setTooltip("Next preset");
    nextButton_.onClick = [this] { stepPreset(1); };
    addAndMakeVisible(nextButton_);

    saveButton_.setTooltip("Save the current sound as a user preset");
    saveButton_.onClick = [this] { savePresetFromUi(); };
    addAndMakeVisible(saveButton_);

    loadButton_.setTooltip("Load a user preset file");
    loadButton_.onClick = [this] { loadPresetFromFileUi(); };
    addAndMakeVisible(loadButton_);

    menuButton_.setTooltip("The whole library, including deleting user presets");
    menuButton_.onClick = [this] { showPresetMenu(); };
    addAndMakeVisible(menuButton_);

    descriptionLabel_.setFont(labelFont(11.0f));
    descriptionLabel_.setColour(juce::Label::textColourId, Palette::displayText.withAlpha(0.62f));
    descriptionLabel_.setJustificationType(juce::Justification::centredLeft);
    addAndMakeVisible(descriptionLabel_);

    inspiredLabel_.setText("INSPIRED BY  SH-101", juce::dontSendNotification);
    inspiredLabel_.setFont(engravedFont(10.5f));
    inspiredLabel_.setColour(juce::Label::textColourId, Palette::accent.withAlpha(0.85f));
    inspiredLabel_.setJustificationType(juce::Justification::centred);
    addAndMakeVisible(inspiredLabel_);

    // ---- Nameplate ---------------------------------------------------------
    nameplateTitle_.setText(juce::String::fromUTF8(JucePlugin_DisplayName), juce::dontSendNotification);
    nameplateTitle_.setFont(boldFont(22.0f));
    nameplateTitle_.setColour(juce::Label::textColourId, Palette::engrave);
    nameplateTitle_.setJustificationType(juce::Justification::centred);
    addAndMakeVisible(nameplateTitle_);

    nameplateSubtitle_.setText("ANALOGUE MONO SYNTHESIZER", juce::dontSendNotification);
    nameplateSubtitle_.setFont(engravedFont(9.5f));
    nameplateSubtitle_.setColour(juce::Label::textColourId, Palette::engraveSoft);
    nameplateSubtitle_.setJustificationType(juce::Justification::centred);
    addAndMakeVisible(nameplateSubtitle_);

    inspiredLabel_.setTooltip("This instrument is a model of the 1982 Roland SH-101: sound and "
                              "behaviour only, with no manufacturer branding.");

    // ---- Panels ------------------------------------------------------------
    addPanel(0, "LFO", { pLfoRate, pLfoWave });
    addPanel(0, "VCO", { pVcoRange, pVcoTune, pVcoModDepth, pPulseWidth, pPwmSource, pPwmAmount });
    addPanel(0, "SOURCE MIXER", { pSawLevel, pPulseLevel, pSubLevel, pSubMode, pNoiseLevel });
    addPanel(0, "VCF", { pCutoff, pResonance, pFilterEnvAmount, pFilterModAmount, pKeyTrack });
    addPanel(0, "VCA", { pVcaMode, pVolume });
    addPanel(0, "ENV", { pAttack, pDecay, pSustain, pRelease, pEnvTrigger });

    addPanel(1, "ARPEGGIATOR", { pArpOn, pArpMode, pArpRate, pArpOctaves });
    addPanel(1, "SEQUENCER", { pSeqOn, pSeqRate });
    addPanel(1, "PERFORMANCE", { pPortamentoMode, pPortamentoTime, pBend });

    refreshPresetList();
    refreshPresetDisplay();
    startTimerHz(12);

    // Sizing last, and only after the controls exist: resized() lays them out, so
    // sizing first would leave every control at zero size (a blank window).
    setSize(kWindowWidth, kWindowHeight);
}

OstraTornAudioProcessorEditor::~OstraTornAudioProcessorEditor() {
    stopTimer();
    setLookAndFeel(nullptr);
}

void OstraTornAudioProcessorEditor::addPanel(int row, const juce::String& title,
                                            const std::vector<int>& paramIds) {
    Panel panel;
    panel.row = row;
    panel.title = title;
    panel.firstControl = static_cast<int>(controls_.size());
    panel.numControls = static_cast<int>(paramIds.size());
    for (int id : paramIds) {
        int itemCount = 0;
        const bool choice = paramChoiceItems(id, itemCount) != nullptr && itemCount > 1;
        panel.contentWidth += choice ? kSwitchCellWidth : kFaderCellWidth;
    }
    panels_.push_back(panel);

    for (int id : paramIds) {
        Control control;
        control.paramId = id;
        const juce::String parameterId = OstraTornAudioProcessor::parameterId(id);
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
            control.slider->setDoubleClickReturnValue(true, getNormalized(SH101Params{}, id));
            control.slider->setTooltip(tooltip);
            addAndMakeVisible(*control.slider);
            control.sliderAttachment =
                std::make_unique<juce::AudioProcessorValueTreeState::SliderAttachment>(
                    processor_.parameters(), parameterId, *control.slider);
            // After the attachment: a SliderAttachment installs its own value->text
            // formatter, which would replace the panel's engineering units.
            control.slider->textFromValueFunction = [id](double v) { return formatParamValue(id, v); };
        }

        control.nameLabel = std::make_unique<juce::Label>();
        control.nameLabel->setText(juce::String(kDisplayName[id]), juce::dontSendNotification);
        control.nameLabel->setJustificationType(juce::Justification::centred);
        control.nameLabel->setColour(juce::Label::textColourId, Palette::engrave);
        control.nameLabel->setInterceptsMouseClicks(false, false);
        addAndMakeVisible(*control.nameLabel);

        controls_.push_back(std::move(control));
    }
}

void OstraTornAudioProcessorEditor::layoutPanel(Panel& panel, juce::Rectangle<int> bounds,
                                               float widthScale) {
    auto inner = bounds.reduced(kPanelPadding);
    inner.removeFromTop(kPanelHeaderHeight);

    const int count = panel.numControls;
    if (count <= 0) return;

    for (int i = 0; i < count; ++i) {
        Control& control = controls_[static_cast<size_t>(panel.firstControl + i)];
        const int nominal = control.isChoice ? kSwitchCellWidth : kFaderCellWidth;
        const int width = juce::jmax(24, static_cast<int>(std::lround(nominal * widthScale)));
        auto cell = inner.removeFromLeft(width);
        auto body = cell.removeFromTop(juce::jmin(kControlHeight, cell.getHeight() - kNameLabelHeight));

        if (control.isChoice) {
            // Switch controls sit on the panel's lower edge, level with the foot of
            // the faders, rather than floating in the middle of the cell.
            const int comboHeight = 22;
            control.combo->setBounds(body.removeFromBottom(comboHeight + 6).reduced(2, 0));
        } else {
            control.slider->setBounds(body.reduced(1, 0));
        }
        control.nameLabel->setBounds(cell.removeFromTop(kNameLabelHeight));
    }
}

void OstraTornAudioProcessorEditor::layoutRow(int row, juce::Rectangle<int> area,
                                              int rightHandBlockWidth) {
    // Panels are sized to their content — a six-control section is wide, a
    // two-control one is narrow — and the row's leftover width becomes the gap
    // between them, the way a hardware panel has sections of different widths
    // sitting side by side.  If the row does not fit, every control is scaled
    // down by the same factor rather than the last section falling off the edge.
    int panelsInRow = 0;
    int contentWidth = 0;
    for (const Panel& panel : panels_) {
        if (panel.row != row) continue;
        ++panelsInRow;
        contentWidth += panel.contentWidth + 2 * kPanelPadding;
    }
    if (contentWidth <= 0 || panelsInRow <= 0) return;

    const int panelAreaWidth = juce::jmax(200, area.getWidth() - rightHandBlockWidth);
    const float widthScale = juce::jmin(1.0f, static_cast<float>(panelAreaWidth)
                                                 / static_cast<float>(contentWidth));
    const int scaledContentWidth = static_cast<int>(std::lround(contentWidth * widthScale));
    const int gap = (panelsInRow > 1)
                        ? juce::jlimit(2, kMaxPanelGap,
                                       (panelAreaWidth - scaledContentWidth) / (panelsInRow - 1))
                        : 0;
    const int usedWidth = scaledContentWidth + gap * (panelsInRow - 1);

    int x = area.getX() + juce::jmax(0, (panelAreaWidth - usedWidth) / 2);
    for (Panel& panel : panels_) {
        if (panel.row != row) continue;

        const int width = static_cast<int>(
            std::lround((panel.contentWidth + 2 * kPanelPadding) * widthScale));
        panel.bounds = juce::Rectangle<int>(x, area.getY(), width, area.getHeight());
        x += width + gap;
        layoutPanel(panel, panel.bounds, widthScale);
    }
}

void OstraTornAudioProcessorEditor::resized() {
    auto area = getLocalBounds().reduced(kMargin);

    // ---- Title bar ---------------------------------------------------------
    auto titleBar = area.removeFromTop(kTitleBarHeight);
    auto titleLeft = titleBar.withTrimmedLeft(18).withTrimmedRight(260);
    titleLabel_.setBounds(titleLeft.removeFromTop(54).withTrimmedTop(6));
    subtitleLabel_.setBounds(titleLeft.removeFromTop(18));

    auto powerArea = titleBar.removeFromRight(240).withSizeKeepingCentre(190, 54);
    auto powerSwitch = powerArea.removeFromRight(46).withSizeKeepingCentre(46, 54);
    powerButton_.setBounds(powerSwitch);

    area.removeFromTop(4);

    // ---- Preset row --------------------------------------------------------
    auto presetBar = area.removeFromTop(kPresetBarHeight);
    presetCaptionLabel_.setBounds(presetBar.removeFromLeft(64).withSizeKeepingCentre(64, 20));
    previousButton_.setBounds(presetBar.removeFromLeft(34).withSizeKeepingCentre(34, 30));
    presetBar.removeFromLeft(4);
    nextButton_.setBounds(presetBar.removeFromLeft(34).withSizeKeepingCentre(34, 30));
    presetBar.removeFromLeft(10);
    presetBox_.setBounds(presetBar.removeFromLeft(330).withSizeKeepingCentre(330, 28));
    presetBar.removeFromLeft(10);
    saveButton_.setBounds(presetBar.removeFromLeft(74).withSizeKeepingCentre(74, 27));
    presetBar.removeFromLeft(6);
    loadButton_.setBounds(presetBar.removeFromLeft(74).withSizeKeepingCentre(74, 27));
    presetBar.removeFromLeft(6);
    menuButton_.setBounds(presetBar.removeFromLeft(74).withSizeKeepingCentre(74, 27));
    presetBar.removeFromLeft(12);
    inspiredLabel_.setBounds(presetBar.removeFromLeft(180).withSizeKeepingCentre(180, 20));
    descriptionLabel_.setBounds(presetBar.reduced(0, 6));

    area.removeFromTop(kRowGap + 2);

    // ---- Rows of sections --------------------------------------------------
    // The lower row keeps a nameplate/status block on the right.
    auto row0 = area.removeFromTop(kRowHeight);
    layoutRow(0, row0, 0);
    area.removeFromTop(kRowGap);
    auto row1 = area.removeFromTop(kRowHeight);
    layoutRow(1, row1, kLowerBlockWidth);

    auto nameplate = nameplateBounds();
    auto nameplateInner = nameplate.reduced(10, 14);
    // Name and strapline sit together, centred on the plate, with the engraved rule
    // underneath them.
    auto textBlock = nameplateInner.withSizeKeepingCentre(nameplateInner.getWidth(), 60);
    nameplateTitle_.setBounds(textBlock.removeFromTop(34));
    nameplateSubtitle_.setBounds(textBlock.removeFromTop(16));
}

juce::Rectangle<int> OstraTornAudioProcessorEditor::lowerRightBlockBounds() const {
    auto area = getLocalBounds().reduced(kMargin);
    area.removeFromTop(kTitleBarHeight + 4 + kPresetBarHeight + kRowGap + 2 + kRowHeight + kRowGap);
    return area.removeFromRight(kLowerBlockWidth).removeFromTop(kRowHeight);
}

juce::Rectangle<int> OstraTornAudioProcessorEditor::nameplateBounds() const {
    return lowerRightBlockBounds().removeFromLeft(240).reduced(0, 22);
}

juce::Rectangle<int> OstraTornAudioProcessorEditor::levelMeterBounds() const {
    auto block = lowerRightBlockBounds();
    block.removeFromLeft(240);
    return block.reduced(10, 14);
}

void OstraTornAudioProcessorEditor::paint(juce::Graphics& g) {
    drawBrushedBody(g, getLocalBounds().toFloat());

    auto area = getLocalBounds().reduced(kMargin);
    const auto titleBar = area.removeFromTop(kTitleBarHeight);
    area.removeFromTop(4);
    const auto presetBar = area.removeFromTop(kPresetBarHeight);

    // Recessed plate for the title bar, engraved rule after the name.
    g.setColour(juce::Colour(0xffc9c9c6).withAlpha(0.55f));
    g.fillRoundedRectangle(titleBar.toFloat(), 4.0f);
    g.setColour(Palette::bevelDark.withAlpha(0.65f));
    g.drawRoundedRectangle(titleBar.toFloat().reduced(0.5f), 4.0f, 1.0f);

    // The rule runs from after the name to the power switch: panel print.
    const int ruleY = titleBar.getY() + 46;
    g.setColour(Palette::engrave);
    g.fillRect(titleBar.getX() + 232, ruleY, titleBar.getRight() - 232 - 250, 2);

    // Preset row: a tinted display strip.
    drawTintedDisplay(g, presetBar.toFloat());
    g.setColour(Palette::bevelLight.withAlpha(0.5f));
    g.drawRoundedRectangle(presetBar.toFloat().reduced(0.5f), 3.0f, 1.0f);

    // Section panels: raised plate, engraved title, hairline under the title.
    for (const Panel& panel : panels_) {
        drawSectionPanel(g, panel.bounds.toFloat(), true);

        auto header = panel.bounds.reduced(kPanelPadding).removeFromTop(kPanelHeaderHeight);
        g.setColour(Palette::engrave);
        g.setFont(engravedFont(11.0f));
        g.drawText(panel.title, header, juce::Justification::centredLeft, false);

        g.setColour(Palette::engraveSoft.withAlpha(0.5f));
        g.fillRect(header.getX(), header.getBottom() - 2, juce::jmin(header.getWidth(), 56), 1);
    }

    // Nameplate: an engraved plate with the instrument's name, and the level meter
    // beside it in the lower-right corner.
    const auto nameplate = nameplateBounds();
    drawSectionPanel(g, nameplate.toFloat(), true);
    g.setColour(Palette::engrave);
    const int nameplateRuleY = nameplate.getBottom() - 30;
    g.fillRect(nameplate.getX() + 14, nameplateRuleY, nameplate.getWidth() - 28, 2);

    paintLevelMeter(g, levelMeterBounds());
    paintTitleBar(g);

    // Panel screws, like the reference panel's four corners.
    const float inset = 6.0f;
    const float radius = 5.0f;
    drawScrew(g, { inset + radius, inset + radius }, radius);
    drawScrew(g, { static_cast<float>(getWidth()) - inset - radius, inset + radius }, radius);
    drawScrew(g, { inset + radius, static_cast<float>(getHeight()) - inset - radius }, radius);
    drawScrew(g, { static_cast<float>(getWidth()) - inset - radius,
                   static_cast<float>(getHeight()) - inset - radius }, radius);
}

void OstraTornAudioProcessorEditor::paintTitleBar(juce::Graphics& g) {
    // Power indicator LED beside the switch, lit whenever the instrument is on.
    auto area = getLocalBounds().reduced(kMargin).removeFromTop(kTitleBarHeight);
    auto powerArea = area.removeFromRight(240).withSizeKeepingCentre(190, 54);
    const juce::Point<float> led(powerArea.getX() + 8.0f, powerArea.getCentreY() + 0.0f);
    drawLed(g, led, 5.0f, Palette::ledRed, ! processor_.isStandby());

    g.setFont(engravedFont(11.0f));
    g.setColour(Palette::engrave);
    g.drawText("POWER", powerArea.getX() + 20, powerArea.getCentreY() - 8, 60, 16,
               juce::Justification::centredLeft, false);
}

void OstraTornAudioProcessorEditor::paintLevelMeter(juce::Graphics& g, juce::Rectangle<int> bounds) {
    // A short ladder: -36, -24, -18, -12, -6, 0 dBFS, green up to -12, amber to
    // -6, red above.  Fed by the processor's block peaks with a decay.
    drawSectionPanel(g, bounds.toFloat(), false);

    const int ledCount = 6;
    const float ledRadius = 5.0f;
    auto inner = bounds.reduced(8, 10);
    auto labels = inner.removeFromRight(30);

    const double thresholds[ledCount] = { -36.0, -24.0, -18.0, -12.0, -6.0, 0.0 };
    const double levelDb = (meterLevel_ > 1.0e-6f) ? 20.0 * std::log10(meterLevel_) : -100.0;

    const int spacing = inner.getHeight() / ledCount;
    for (int i = 0; i < ledCount; ++i) {
        const int index = ledCount - 1 - i;   // brightest at the top of the ladder
        const auto colour = (index >= 5) ? Palette::ledRed
                            : (index >= 4) ? Palette::ledAmber
                            : (index >= 3) ? Palette::ledYellow
                                           : Palette::ledGreen;
        const juce::Point<float> centre(inner.getCentreX(),
                                        static_cast<float>(inner.getY() + spacing * i + spacing / 2));
        drawLed(g, centre, ledRadius, colour, levelDb >= thresholds[index]);

        g.setFont(labelFont(9.0f));
        g.setColour(Palette::engraveSoft);
        g.drawText(juce::String(juce::roundToInt(thresholds[index])),
                   labels.getX(), static_cast<int>(centre.y) - 7, labels.getWidth(), 14,
                   juce::Justification::centredLeft, false);
    }
}

// ---- Presets ----------------------------------------------------------------
int OstraTornAudioProcessorEditor::combinedPresetCount() const {
    return numPresets() + userPresets_.size();
}

void OstraTornAudioProcessorEditor::refreshPresetList() {
    userPresets_ = processor_.userPresetNames();

    presetBox_.clear(juce::dontSendNotification);
    for (int i = 0; i < numPresets(); ++i) {
        presetBox_.addItem(juce::String(preset(i).category) + ": " + preset(i).name, i + 1);
    }
    for (int i = 0; i < userPresets_.size(); ++i) {
        presetBox_.addItem("USER: " + userPresets_[i], kUserPresetIdBase + i + 1);
    }
    refreshPresetDisplay();
}

void OstraTornAudioProcessorEditor::updatePresetDescription() {
    const bool modified = processor_.presetIsModified();
    if (modified) {
        descriptionLabel_.setText("modified - press SAVE to keep it", juce::dontSendNotification);
        return;
    }
    if (processor_.currentPresetIsUser()) {
        descriptionLabel_.setText("user preset: " + processor_.currentPresetName(),
                                  juce::dontSendNotification);
        return;
    }
    const int index = juce::jlimit(0, numPresets() - 1, processor_.currentPresetIndex());
    descriptionLabel_.setText(juce::String(preset(index).description), juce::dontSendNotification);
}

void OstraTornAudioProcessorEditor::refreshPresetDisplay() {
    const juce::String name = processor_.currentPresetName();
    const bool modified = processor_.presetIsModified();
    const juce::String displayText = (modified ? "* " : "") + name;

    // Find the entry for what is loaded: a factory preset by index, a user preset
    // by name (the file name is sanitised, the display name is what was typed).
    if (processor_.currentPresetIsUser()) {
        const int index = userPresets_.indexOf(name);
        if (index >= 0) {
            presetBox_.setSelectedId(kUserPresetIdBase + index + 1, juce::dontSendNotification);
        } else {
            presetBox_.setSelectedId(0, juce::dontSendNotification);
            presetBox_.setText(displayText, juce::dontSendNotification);
        }
    } else {
        const int index = juce::jlimit(0, numPresets() - 1, processor_.currentPresetIndex());
        presetBox_.setSelectedId(index + 1, juce::dontSendNotification);
    }

    updatePresetDescription();
}

void OstraTornAudioProcessorEditor::loadPresetFromUi(int combinedIndex) {
    if (combinedIndex < 0) return;
    if (combinedIndex < numPresets()) {
        processor_.loadPreset(combinedIndex);
    } else {
        const int userIndex = combinedIndex - numPresets();
        if (userIndex < userPresets_.size()) processor_.loadUserPreset(userPresets_[userIndex]);
    }
    refreshPresetDisplay();
}

void OstraTornAudioProcessorEditor::stepPreset(int delta) {
    const int count = combinedPresetCount();
    if (count <= 0) return;

    int current = processor_.currentPresetIndex();
    if (processor_.currentPresetIsUser()) {
        const int index = userPresets_.indexOf(processor_.currentPresetName());
        current = (index >= 0) ? (numPresets() + index) : current;
    }
    int next = (current + delta) % count;
    if (next < 0) next += count;
    loadPresetFromUi(next);
}

void OstraTornAudioProcessorEditor::savePresetFromUi() {
    auto* window = new juce::AlertWindow("Save preset",
                                         "Name for this sound:",
                                         juce::MessageBoxIconType::NoIcon);
    window->addTextEditor("name", processor_.currentPresetName());
    window->addButton("Save", 1, juce::KeyPress(juce::KeyPress::returnKey));
    window->addButton("Cancel", 0, juce::KeyPress(juce::KeyPress::escapeKey));

    window->enterModalState(true, juce::ModalCallbackFunction::create([this, window](int result) {
        if (result == 1) {
            const juce::String name = window->getTextEditorContents("name").trim();
            if (name.isNotEmpty()) {
                if (processor_.saveUserPreset(name)) {
                    refreshPresetList();
                    refreshPresetDisplay();
                }
            }
        }
    }), true);
}

void OstraTornAudioProcessorEditor::loadPresetFromFileUi() {
    auto directory = OstraTornAudioProcessor::userPresetDirectory();
    if (! directory.exists()) directory = juce::File::getSpecialLocation(juce::File::userHomeDirectory);

    fileChooser_ = std::make_unique<juce::FileChooser>(
        "Load preset", directory, juce::String("*.otp101"));

    fileChooser_->launchAsync(juce::FileBrowserComponent::openMode
                                  | juce::FileBrowserComponent::canSelectFiles,
                              [this](const juce::FileChooser& chooser) {
        const juce::File file = chooser.getResult();
        if (file.existsAsFile() && processor_.loadUserPresetFile(file)) {
            refreshPresetList();
            refreshPresetDisplay();
        }
    });
}

void OstraTornAudioProcessorEditor::deleteUserPresetUi(const juce::String& name) {
    juce::AlertWindow::showOkCancelBox(
        juce::MessageBoxIconType::WarningIcon, "Delete preset",
        "Delete the user preset \"" + name + "\"?", "Delete", "Cancel", this,
        juce::ModalCallbackFunction::create([this, name](int result) {
            if (result != 0 && processor_.deleteUserPreset(name)) {
                refreshPresetList();
                refreshPresetDisplay();
            }
        }));
}

void OstraTornAudioProcessorEditor::showPresetMenu() {
    juce::PopupMenu menu;
    menu.setLookAndFeel(&lookAndFeel_);

    juce::PopupMenu factory;
    juce::String currentCategory;
    for (int i = 0; i < numPresets(); ++i) {
        const juce::String category = juce::String(preset(i).category);
        if (category != currentCategory) {
            currentCategory = category;
            factory.addSectionHeader(category);
        }
        factory.addItem(i + 1, juce::String(preset(i).name),
                        true, juce::String(preset(i).name) == processor_.currentPresetName());
    }
    menu.addSubMenu("Factory (" + juce::String(numPresets()) + ")", factory);

    juce::PopupMenu user;
    if (userPresets_.isEmpty()) {
        user.addItem(kUserPresetIdBase + 9000, "no user presets yet", false, false);
    } else {
        for (int i = 0; i < userPresets_.size(); ++i) {
            user.addItem(kUserPresetIdBase + 1 + i, userPresets_[i], true,
                         userPresets_[i] == processor_.currentPresetName());
        }
    }
    menu.addSubMenu("User (" + juce::String(userPresets_.size()) + ")", user);

    menu.addSeparator();
    menu.addItem(kUserPresetIdBase + 9001, "Save current sound...");
    menu.addItem(kUserPresetIdBase + 9002, "Load preset file...");
    if (! userPresets_.isEmpty()) {
        juce::PopupMenu removeMenu;
        for (int i = 0; i < userPresets_.size(); ++i) {
            removeMenu.addItem(kUserPresetIdBase + 5000 + i, userPresets_[i]);
        }
        menu.addSubMenu("Delete user preset", removeMenu);
    }
    menu.addItem(kUserPresetIdBase + 9003, "Open preset folder");

    menu.showMenuAsync(juce::PopupMenu::Options().withTargetComponent(&menuButton_),
                       [this](int result) {
        if (result <= 0) return;
        if (result < kUserPresetIdBase) {
            loadPresetFromUi(result - 1);
        } else if (result >= kUserPresetIdBase + 5000 && result < kUserPresetIdBase + 6000) {
            const int index = result - (kUserPresetIdBase + 5000);
            if (index < userPresets_.size()) deleteUserPresetUi(userPresets_[index]);
        } else if (result >= kUserPresetIdBase + 1 && result < kUserPresetIdBase + 1 + 1000) {
            const int index = result - (kUserPresetIdBase + 1);
            if (index < userPresets_.size()) {
                processor_.loadUserPreset(userPresets_[index]);
                refreshPresetDisplay();
            }
        } else if (result == kUserPresetIdBase + 9001) {
            savePresetFromUi();
        } else if (result == kUserPresetIdBase + 9002) {
            loadPresetFromFileUi();
        } else if (result == kUserPresetIdBase + 9003) {
            OstraTornAudioProcessor::userPresetDirectory().createDirectory();
            OstraTornAudioProcessor::userPresetDirectory().revealToUser();
        }
    });
}

void OstraTornAudioProcessorEditor::timerCallback() {
    // Level meter: follow the processor's block peak with a meter-style decay.
    const float level = processor_.outputLevel();
    meterLevel_ = (level > meterLevel_) ? level : (meterLevel_ * 0.82f);

    // The host (or automation) can change programs behind our back, so the combo
    // follows the processor rather than the other way round.
    const int expectedId = [this] {
        if (processor_.currentPresetIsUser()) {
            const int index = userPresets_.indexOf(processor_.currentPresetName());
            return (index >= 0) ? (kUserPresetIdBase + index + 1) : -1;
        }
        return processor_.currentPresetIndex() + 1;
    }();

    if (expectedId != -1 && presetBox_.getSelectedId() != expectedId) {
        refreshPresetDisplay();
    } else {
        updatePresetDescription();     // keeps the "modified" marker honest
    }

    if (powerButton_.getToggleState() == processor_.isStandby()) {
        powerButton_.setToggleState(! processor_.isStandby(), juce::dontSendNotification);
    }

    repaint();
}
