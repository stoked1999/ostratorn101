#include "SH101LookAndFeel.h"

namespace sh101ui {

const juce::Colour Palette::windowBg     { 0xff15161a };
const juce::Colour Palette::panelBg      { 0xff23252c };
const juce::Colour Palette::panelOutline { 0xff3a3d47 };
const juce::Colour Palette::headerText   { 0xffffa63a };
const juce::Colour Palette::text         { 0xffe9e6df };
const juce::Colour Palette::textDim      { 0xff918e88 };
const juce::Colour Palette::accent       { 0xffffa63a };
const juce::Colour Palette::accentDim    { 0xff7c501c };
const juce::Colour Palette::trackBg      { 0xff101114 };
const juce::Colour Palette::trackFill    { 0xffc07a24 };
const juce::Colour Palette::buttonBg     { 0xff2d3038 };

} // namespace sh101ui

using namespace sh101ui;

SH101LookAndFeel::SH101LookAndFeel() {
    setColour(juce::Label::textColourId, Palette::text);
    setColour(juce::Label::backgroundColourId, juce::Colours::transparentBlack);
    setColour(juce::Label::outlineColourId, juce::Colours::transparentBlack);

    setColour(juce::Slider::textBoxTextColourId, Palette::text);
    setColour(juce::Slider::textBoxBackgroundColourId, juce::Colours::transparentBlack);
    setColour(juce::Slider::textBoxOutlineColourId, juce::Colours::transparentBlack);
    setColour(juce::Slider::textBoxHighlightColourId, Palette::accent.withAlpha(0.35f));
    setColour(juce::Slider::thumbColourId, Palette::accent);
    setColour(juce::Slider::trackColourId, Palette::trackFill);
    setColour(juce::Slider::backgroundColourId, Palette::trackBg);

    // The combo boxes draw their own background in drawComboBox(), so the
    // inherited colours are transparent to avoid double-drawing.
    setColour(juce::ComboBox::backgroundColourId, juce::Colours::transparentBlack);
    setColour(juce::ComboBox::outlineColourId, juce::Colours::transparentBlack);
    setColour(juce::ComboBox::textColourId, Palette::text);
    setColour(juce::ComboBox::arrowColourId, Palette::accent);

    setColour(juce::PopupMenu::backgroundColourId, Palette::panelBg);
    setColour(juce::PopupMenu::textColourId, Palette::text);
    setColour(juce::PopupMenu::highlightedBackgroundColourId, Palette::accent.withAlpha(0.30f));
    setColour(juce::PopupMenu::highlightedTextColourId, Palette::text);

    setColour(juce::TextButton::buttonColourId, Palette::buttonBg);
    setColour(juce::TextButton::buttonOnColourId, Palette::accentDim);
    setColour(juce::TextButton::textColourOffId, Palette::text);
    setColour(juce::TextButton::textColourOnId, Palette::text);

    setColour(juce::TooltipWindow::backgroundColourId, Palette::panelBg);
    setColour(juce::TooltipWindow::textColourId, Palette::text);
    setColour(juce::TooltipWindow::outlineColourId, Palette::panelOutline);
}

void SH101LookAndFeel::drawLinearSlider(juce::Graphics& g, int x, int y, int width, int height,
                                        float sliderPos, float, float,
                                        juce::Slider::SliderStyle style, juce::Slider& slider) {
    if (style != juce::Slider::LinearVertical) {
        LookAndFeel_V4::drawLinearSlider(g, x, y, width, height, sliderPos, 0.0f, 0.0f, style, slider);
        return;
    }

    // The value read-out is placed by getSliderLayout() below the groove, so the
    // whole rectangle passed in here is fader.
    const float trackTop = static_cast<float>(y) + 4.0f;
    const float trackBottom = static_cast<float>(y + height) - 4.0f;
    if (trackBottom <= trackTop) return;

    const float centreX = static_cast<float>(x) + static_cast<float>(width) * 0.5f;
    const float trackWidth = 6.0f;
    const juce::Rectangle<float> track(centreX - trackWidth * 0.5f, trackTop, trackWidth,
                                       trackBottom - trackTop);
    const float clampedPos = juce::jlimit(track.getY(), track.getBottom(), sliderPos);

    g.setColour(Palette::trackBg);
    g.fillRoundedRectangle(track, 3.0f);

    // Filled from the bottom of the groove up to the cap: the panel reads at a
    // glance, like the painted area next to a physical fader slot.
    const juce::Rectangle<float> fill(track.getX(), clampedPos, track.getWidth(),
                                      track.getBottom() - clampedPos);
    if (fill.getHeight() > 0.5f) {
        g.setColour(Palette::trackFill);
        g.fillRoundedRectangle(fill, 3.0f);
    }

    g.setColour(Palette::panelOutline);
    g.drawRoundedRectangle(track, 3.0f, 1.0f);

    const float capWidth = juce::jmin(static_cast<float>(width) - 2.0f, 30.0f);
    const float capHeight = 12.0f;
    const juce::Rectangle<float> cap(centreX - capWidth * 0.5f, clampedPos - capHeight * 0.5f,
                                     capWidth, capHeight);
    g.setColour(slider.isMouseOverOrDragging() ? Palette::accent.brighter(0.15f) : Palette::accent);
    g.fillRoundedRectangle(cap, 3.0f);
    g.setColour(Palette::accent.darker(0.7f));
    g.drawRoundedRectangle(cap, 3.0f, 1.0f);
    g.setColour(Palette::windowBg.withAlpha(0.85f));
    g.fillRect(cap.getX() + 2.5f, cap.getCentreY() - 0.75f, cap.getWidth() - 5.0f, 1.5f);

    // The value read-out sits in the strip the layout reserved below the groove.
    drawSliderReadOut(g, { x, y + height, width, kReadoutHeight }, slider,
                      slider.isMouseOverOrDragging());
}

void SH101LookAndFeel::drawSliderReadOut(juce::Graphics& g, juce::Rectangle<int> area,
                                         juce::Slider& slider, bool highlighted) {
    g.setColour(highlighted ? Palette::text : Palette::text.withAlpha(0.85f));
    g.setFont(panelFont(11.0f));
    g.drawText(slider.getTextFromValue(slider.getValue()), area, juce::Justification::centred, false);
}

void SH101LookAndFeel::drawComboBox(juce::Graphics& g, int width, int height, bool,
                                    int, int, int, int, juce::ComboBox& box) {
    const juce::Rectangle<float> r(0.5f, 0.5f, static_cast<float>(width) - 1.0f,
                                   static_cast<float>(height) - 1.0f);
    g.setColour(Palette::trackBg);
    g.fillRoundedRectangle(r, 3.0f);
    g.setColour(box.isMouseOverOrDragging() || box.hasKeyboardFocus(false) ? Palette::accent
                                                                           : Palette::panelOutline);
    g.drawRoundedRectangle(r, 3.0f, 1.0f);

    juce::Path arrow;
    const float ax = static_cast<float>(width) - 11.0f;
    const float ay = static_cast<float>(height) * 0.5f;
    arrow.addTriangle(ax - 4.0f, ay - 2.0f, ax + 4.0f, ay - 2.0f, ax, ay + 3.0f);
    g.setColour(Palette::accent);
    g.fillPath(arrow);
}

void SH101LookAndFeel::positionComboBoxText(juce::ComboBox& box, juce::Label& label) {
    label.setBounds(7, 1, box.getWidth() - 22, box.getHeight() - 2);
    label.setFont(panelFont(11.5f));
    label.setJustificationType(juce::Justification::centredLeft);
}

juce::Slider::SliderLayout SH101LookAndFeel::getSliderLayout(juce::Slider& slider) {
    if (slider.getSliderStyle() != juce::Slider::LinearVertical) {
        return LookAndFeel_V4::getSliderLayout(slider);
    }

    juce::Slider::SliderLayout layout;
    auto bounds = slider.getLocalBounds();
    // The read-out strip is always reserved for a vertical fader: the value is
    // drawn by drawLinearSlider() rather than by a text-box component, so the
    // engineering-unit text is under this look-and-feel's control.
    if (bounds.getHeight() > kReadoutHeight + 16) {
        bounds.removeFromBottom(kReadoutHeight);
    }
    layout.sliderBounds = bounds;
    return layout;
}

void SH101LookAndFeel::drawButtonBackground(juce::Graphics& g, juce::Button& button,
                                            const juce::Colour&, bool highlighted, bool down) {
    const juce::Rectangle<float> r(0.5f, 0.5f, static_cast<float>(button.getWidth()) - 1.0f,
                                   static_cast<float>(button.getHeight()) - 1.0f);
    juce::Colour fill = Palette::buttonBg;
    if (down) fill = Palette::accentDim;
    else if (highlighted) fill = Palette::buttonBg.brighter(0.18f);

    g.setColour(fill);
    g.fillRoundedRectangle(r, 4.0f);
    g.setColour(highlighted || down ? Palette::accent : Palette::panelOutline);
    g.drawRoundedRectangle(r, 4.0f, 1.0f);
}

juce::Font SH101LookAndFeel::getLabelFont(juce::Label& label) {
    // A label whose parent is a slider is that slider's value read-out; the rest
    // are control names.
    const bool isValue = dynamic_cast<juce::Slider*>(label.getParentComponent()) != nullptr;
    return panelFont(isValue ? 11.5f : 10.5f);
}

juce::Font SH101LookAndFeel::getComboBoxFont(juce::ComboBox&) {
    return panelFont(11.5f);
}

juce::Font SH101LookAndFeel::getPopupMenuFont() {
    return panelFont(12.5f);
}

juce::Font SH101LookAndFeel::getTextButtonFont(juce::TextButton&, int buttonHeight) {
    return panelFontBold(juce::jmin(16.0f, static_cast<float>(buttonHeight) * 0.6f));
}
