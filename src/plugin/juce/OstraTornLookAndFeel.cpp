#include "OstraTornLookAndFeel.h"

namespace ostra {

// Brushed aluminium body with a warm-grey tint, the way the reference panel reads.
const juce::Colour Palette::bodyTop        { 0xffd9d9d6 };
const juce::Colour Palette::bodyBottom     { 0xffb3b3b0 };
const juce::Colour Palette::panelTop       { 0xffe4e4e1 };
const juce::Colour Palette::panelBottom    { 0xffc4c4c1 };
const juce::Colour Palette::bevelLight     { 0xfff4f4f2 };
const juce::Colour Palette::bevelDark      { 0xff8b8b89 };
const juce::Colour Palette::engrave        { 0xff24242a };
const juce::Colour Palette::engraveSoft    { 0xff5d5d63 };
const juce::Colour Palette::displayTop     { 0xff2c2c30 };
const juce::Colour Palette::displayBottom  { 0xff101013 };
const juce::Colour Palette::displayText    { 0xffe0e0da };
const juce::Colour Palette::accent         { 0xffe8862a };
const juce::Colour Palette::accentDark     { 0xff9c5416 };
const juce::Colour Palette::slotDark       { 0xff3a3a3d };
const juce::Colour Palette::capTop         { 0xff4a4a4f };
const juce::Colour Palette::capBottom      { 0xff232326 };
const juce::Colour Palette::ledRed         { 0xffe0402c };
const juce::Colour Palette::ledAmber       { 0xfff0932a };
const juce::Colour Palette::ledYellow      { 0xffe8d24a };
const juce::Colour Palette::ledGreen       { 0xff5cc465 };
const juce::Colour Palette::screw          { 0xffa9a9a6 };

// ---- Hardware drawing -------------------------------------------------------

void drawBrushedBody(juce::Graphics& g, juce::Rectangle<float> area) {
    g.setGradientFill(juce::ColourGradient(Palette::bodyTop, area.getX(), area.getY(),
                                           Palette::bodyBottom, area.getX(), area.getBottom(), false));
    g.fillRect(area);

    // Brushed texture: very faint horizontal hairlines.  Drawn deterministically
    // (every third pixel row) so the panel looks the same on every repaint.
    g.setColour(juce::Colours::white.withAlpha(0.05f));
    for (float y = area.getY() + 1.0f; y < area.getBottom(); y += 3.0f) {
        g.fillRect(area.getX(), y, area.getWidth(), 1.0f);
    }
    g.setColour(juce::Colours::black.withAlpha(0.045f));
    for (float y = area.getY() + 2.0f; y < area.getBottom(); y += 3.0f) {
        g.fillRect(area.getX(), y, area.getWidth(), 1.0f);
    }
}

void drawSectionPanel(juce::Graphics& g, juce::Rectangle<float> area, bool raised) {
    const float corner = 4.0f;
    const juce::Colour top = raised ? Palette::panelTop : Palette::panelTop.darker(0.12f);
    const juce::Colour bottom = raised ? Palette::panelBottom : Palette::panelBottom.darker(0.10f);
    g.setGradientFill(juce::ColourGradient(top, area.getX(), area.getY(), bottom, area.getX(),
                                           area.getBottom(), false));
    g.fillRoundedRectangle(area, corner);

    // Bevel: bright along the top/left, shadow along the bottom/right.
    g.setColour(Palette::bevelLight);
    g.drawLine(area.getX() + corner, area.getY() + 0.5f, area.getRight() - corner, area.getY() + 0.5f, 1.0f);
    g.drawLine(area.getX() + 0.5f, area.getY() + corner, area.getX() + 0.5f, area.getBottom() - corner, 1.0f);
    g.setColour(Palette::bevelDark);
    g.drawLine(area.getX() + corner, area.getBottom() - 0.5f, area.getRight() - corner, area.getBottom() - 0.5f, 1.0f);
    g.drawLine(area.getRight() - 0.5f, area.getY() + corner, area.getRight() - 0.5f, area.getBottom() - corner, 1.0f);
}

void drawTintedDisplay(juce::Graphics& g, juce::Rectangle<float> area) {
    const float corner = 2.5f;
    g.setGradientFill(juce::ColourGradient(Palette::displayTop, area.getX(), area.getY(),
                                           Palette::displayBottom, area.getX(), area.getBottom(), false));
    g.fillRoundedRectangle(area, corner);
    // Inner shadow at the top and a hairline edge: a window set into the metal.
    g.setColour(juce::Colours::black.withAlpha(0.55f));
    g.drawRoundedRectangle(area.reduced(0.5f), corner, 1.0f);
    g.setColour(juce::Colours::white.withAlpha(0.10f));
    g.drawLine(area.getX() + corner, area.getBottom() - 0.5f, area.getRight() - corner,
               area.getBottom() - 0.5f, 1.0f);
}

void drawScrew(juce::Graphics& g, juce::Point<float> centre, float radius) {
    // Recessed screw: dark well, metal head, slot.
    g.setColour(juce::Colours::black.withAlpha(0.30f));
    g.fillEllipse(centre.x - radius, centre.y - radius, radius * 2.0f, radius * 2.0f);

    const juce::Rectangle<float> head(centre.x - radius * 0.8f, centre.y - radius * 0.8f,
                                      radius * 1.6f, radius * 1.6f);
    g.setGradientFill(juce::ColourGradient(Palette::screw.brighter(0.35f), head.getX(), head.getY(),
                                           Palette::screw.darker(0.45f), head.getRight(),
                                           head.getBottom(), false));
    g.fillEllipse(head);

    g.setColour(juce::Colours::black.withAlpha(0.55f));
    juce::Path slot;
    slot.addRectangle(head.getX() + head.getWidth() * 0.18f, centre.y - 0.7f,
                      head.getWidth() * 0.64f, 1.4f);
    g.fillPath(slot);
}

void drawLed(juce::Graphics& g, juce::Point<float> centre, float radius, juce::Colour colour,
             bool lit) {
    const juce::Colour body = lit ? colour : colour.withMultipliedBrightness(0.32f).withSaturation(0.45f);
    if (lit) {
        // A lit LED gets a halo, which is what makes a panel read as "on".
        g.setColour(colour.withAlpha(0.30f));
        g.fillEllipse(centre.x - radius * 2.1f, centre.y - radius * 2.1f, radius * 4.2f, radius * 4.2f);
    }
    g.setColour(juce::Colours::black.withAlpha(0.45f));
    g.fillEllipse(centre.x - radius - 0.6f, centre.y - radius - 0.6f, radius * 2.0f + 1.2f,
                  radius * 2.0f + 1.2f);
    g.setGradientFill(juce::ColourGradient(body.brighter(lit ? 0.45f : 0.25f), centre.x - radius * 0.4f,
                                           centre.y - radius * 0.6f, body.darker(0.4f),
                                           centre.x + radius * 0.5f, centre.y + radius * 0.7f, false));
    g.fillEllipse(centre.x - radius, centre.y - radius, radius * 2.0f, radius * 2.0f);
}

} // namespace ostra

using namespace ostra;

OstraTornLookAndFeel::OstraTornLookAndFeel() {
    setColour(juce::Label::textColourId, Palette::engrave);
    setColour(juce::Label::backgroundColourId, juce::Colours::transparentBlack);
    setColour(juce::Label::outlineColourId, juce::Colours::transparentBlack);

    setColour(juce::Slider::textBoxTextColourId, Palette::displayText);
    setColour(juce::Slider::textBoxBackgroundColourId, juce::Colours::transparentBlack);
    setColour(juce::Slider::textBoxOutlineColourId, juce::Colours::transparentBlack);
    setColour(juce::Slider::textBoxHighlightColourId, Palette::accent.withAlpha(0.35f));
    setColour(juce::Slider::thumbColourId, Palette::accent);
    setColour(juce::Slider::trackColourId, Palette::accent);
    setColour(juce::Slider::backgroundColourId, Palette::slotDark);

    // Combo boxes draw their own tinted window, so the inherited colours are
    // transparent to avoid double-drawing.
    setColour(juce::ComboBox::backgroundColourId, juce::Colours::transparentBlack);
    setColour(juce::ComboBox::outlineColourId, juce::Colours::transparentBlack);
    setColour(juce::ComboBox::textColourId, Palette::displayText);
    setColour(juce::ComboBox::arrowColourId, Palette::accent);

    setColour(juce::PopupMenu::backgroundColourId, juce::Colour(0xff2a2a2e));
    setColour(juce::PopupMenu::textColourId, Palette::displayText);
    setColour(juce::PopupMenu::highlightedBackgroundColourId, Palette::accent.withAlpha(0.35f));
    setColour(juce::PopupMenu::highlightedTextColourId, juce::Colours::white);

    setColour(juce::TextButton::buttonColourId, juce::Colour(0xffd2d2cf));
    setColour(juce::TextButton::buttonOnColourId, Palette::accentDark);
    setColour(juce::TextButton::textColourOffId, Palette::engrave);
    setColour(juce::TextButton::textColourOnId, juce::Colours::white);

    setColour(juce::TooltipWindow::backgroundColourId, juce::Colour(0xff2a2a2e));
    setColour(juce::TooltipWindow::textColourId, Palette::displayText);
    setColour(juce::TooltipWindow::outlineColourId, Palette::bevelDark);
}

void OstraTornLookAndFeel::drawLinearSlider(juce::Graphics& g, int x, int y, int width, int height,
                                            float sliderPos, float, float,
                                            juce::Slider::SliderStyle style, juce::Slider& slider) {
    if (style != juce::Slider::LinearVertical) {
        LookAndFeel_V4::drawLinearSlider(g, x, y, width, height, sliderPos, 0.0f, 0.0f, style, slider);
        return;
    }

    const float trackTop = static_cast<float>(y) + 3.0f;
    const float trackBottom = static_cast<float>(y + height) - 3.0f;
    if (trackBottom <= trackTop) return;

    const float centreX = static_cast<float>(x) + static_cast<float>(width) * 0.5f;
    const float slotWidth = 12.0f;
    const juce::Rectangle<float> slot(centreX - slotWidth * 0.5f, trackTop, slotWidth,
                                      trackBottom - trackTop);
    const float clampedPos = juce::jlimit(trackTop, trackBottom, sliderPos);

    // Recessed slot: dark well with a shadowed top edge and a lit bottom edge.
    g.setGradientFill(juce::ColourGradient(Palette::slotDark.darker(0.55f), slot.getX(), slot.getY(),
                                           Palette::slotDark.brighter(0.12f), slot.getRight(),
                                           slot.getY(), false));
    g.fillRoundedRectangle(slot, 3.0f);
    g.setColour(juce::Colours::black.withAlpha(0.45f));
    g.drawLine(slot.getX() + 1.0f, slot.getY() + 0.5f, slot.getRight() - 1.0f, slot.getY() + 0.5f, 1.0f);
    g.setColour(juce::Colours::white.withAlpha(0.14f));
    g.drawLine(slot.getX() + 1.0f, slot.getBottom() - 0.5f, slot.getRight() - 1.0f,
               slot.getBottom() - 0.5f, 1.0f);

    // Scale ticks either side of the slot.
    g.setColour(Palette::engraveSoft.withAlpha(0.65f));
    for (int tick = 0; tick <= 10; ++tick) {
        const float ty = trackTop + (trackBottom - trackTop) * (1.0f - static_cast<float>(tick) / 10.0f);
        const float length = (tick % 5 == 0) ? 5.0f : 3.0f;
        g.fillRect(slot.getX() - length - 2.0f, ty - 0.5f, length, 1.0f);
        g.fillRect(slot.getRight() + 2.0f, ty - 0.5f, length, 1.0f);
    }

    // Fader cap: dark body, orange band, bright top edge — the signature look.
    const float capWidth = juce::jmin(static_cast<float>(width) - 6.0f, 30.0f);
    const float capHeight = 21.0f;
    const juce::Rectangle<float> cap(centreX - capWidth * 0.5f, clampedPos - capHeight * 0.5f,
                                     capWidth, capHeight);

    g.setColour(juce::Colours::black.withAlpha(0.35f));      // drop shadow
    g.fillRoundedRectangle(cap.translated(0.0f, 1.5f), 3.0f);
    g.setGradientFill(juce::ColourGradient(Palette::capTop, cap.getX(), cap.getY(),
                                           Palette::capBottom, cap.getX(), cap.getBottom(), false));
    g.fillRoundedRectangle(cap, 3.0f);

    const juce::Rectangle<float> band(cap.getX() + 1.0f, cap.getCentreY() - 3.2f, cap.getWidth() - 2.0f,
                                      6.4f);
    g.setColour(slider.isMouseOverOrDragging() ? Palette::accent.brighter(0.18f) : Palette::accent);
    g.fillRect(band);

    g.setColour(juce::Colours::white.withAlpha(0.22f));
    g.drawLine(cap.getX() + 2.0f, cap.getY() + 1.0f, cap.getRight() - 2.0f, cap.getY() + 1.0f, 1.0f);
    g.setColour(juce::Colours::black.withAlpha(0.55f));
    g.drawRoundedRectangle(cap.reduced(0.5f), 3.0f, 1.0f);

    drawSliderReadOut(g, { x, y + height, width, kReadoutHeight }, slider,
                      slider.isMouseOverOrDragging());
}

void OstraTornLookAndFeel::drawSliderReadOut(juce::Graphics& g, juce::Rectangle<int> area,
                                             juce::Slider& slider, bool highlighted) {
    // Value in the tinted-window colour scheme, directly under the fader.
    g.setColour(highlighted ? Palette::engrave : Palette::engraveSoft);
    g.setFont(labelFont(11.0f));
    g.drawText(slider.getTextFromValue(slider.getValue()), area, juce::Justification::centred, false);
}

void OstraTornLookAndFeel::drawComboBox(juce::Graphics& g, int width, int height, bool,
                                        int, int, int, int, juce::ComboBox& box) {
    const juce::Rectangle<float> r(0.5f, 0.5f, static_cast<float>(width) - 1.0f,
                                   static_cast<float>(height) - 1.0f);
    // Switch controls are drawn as small tinted windows, matching the reference
    // panel's drop-downs.
    g.setGradientFill(juce::ColourGradient(Palette::displayTop, r.getX(), r.getY(),
                                           Palette::displayBottom, r.getX(), r.getBottom(), false));
    g.fillRoundedRectangle(r, 2.5f);
    g.setColour(juce::Colours::black.withAlpha(0.5f));
    g.drawRoundedRectangle(r, 2.5f, 1.0f);

    juce::Path arrow;
    const float ax = static_cast<float>(width) - 10.0f;
    const float ay = static_cast<float>(height) * 0.5f;
    arrow.addTriangle(ax - 4.0f, ay - 2.0f, ax + 4.0f, ay - 2.0f, ax, ay + 3.0f);
    g.setColour(box.isMouseOverOrDragging() ? Palette::accent.brighter(0.25f) : Palette::accent);
    g.fillPath(arrow);
}

void OstraTornLookAndFeel::positionComboBoxText(juce::ComboBox& box, juce::Label& label) {
    label.setBounds(5, 0, box.getWidth() - 20, box.getHeight());
    label.setFont(labelFont(11.5f));
    label.setJustificationType(juce::Justification::centred);
}

juce::Slider::SliderLayout OstraTornLookAndFeel::getSliderLayout(juce::Slider& slider) {
    if (slider.getSliderStyle() != juce::Slider::LinearVertical) {
        return LookAndFeel_V4::getSliderLayout(slider);
    }
    juce::Slider::SliderLayout layout;
    auto bounds = slider.getLocalBounds();
    if (bounds.getHeight() > kReadoutHeight + 24) bounds.removeFromBottom(kReadoutHeight);
    layout.sliderBounds = bounds;
    return layout;
}

void OstraTornLookAndFeel::drawButtonBackground(juce::Graphics& g, juce::Button& button,
                                                const juce::Colour&, bool highlighted, bool down) {
    const juce::Rectangle<float> r(0.5f, 0.5f, static_cast<float>(button.getWidth()) - 1.0f,
                                   static_cast<float>(button.getHeight()) - 1.0f);
    const bool accentText = button.getProperties()[accentTextProperty];
    juce::Colour light = juce::Colour(0xffdededb);
    juce::Colour dark = juce::Colour(0xffbcbcb9);
    if (accentText) {
        light = juce::Colour(0xffe2e2df);
        dark = juce::Colour(0xffc2c2bf);
    }
    if (down) { light = dark; dark = dark.darker(0.15f); }
    else if (highlighted) { light = light.brighter(0.08f); dark = dark.brighter(0.05f); }

    g.setGradientFill(juce::ColourGradient(light, r.getX(), r.getY(), dark, r.getX(), r.getBottom(),
                                           false));
    g.fillRoundedRectangle(r, 3.0f);
    g.setColour(Palette::bevelLight);
    g.drawLine(r.getX() + 3.0f, r.getY() + 0.5f, r.getRight() - 3.0f, r.getY() + 0.5f, 1.0f);
    g.setColour(Palette::bevelDark);
    g.drawRoundedRectangle(r.reduced(0.5f), 3.0f, 1.0f);
}

void OstraTornLookAndFeel::drawButtonText(juce::Graphics& g, juce::TextButton& button, bool, bool) {
    const bool accentText = button.getProperties()[accentTextProperty];
    const bool down = button.isDown();
    g.setFont(getTextButtonFont(button, button.getHeight()));
    g.setColour(down ? juce::Colours::white
                     : (accentText ? Palette::accentDark : Palette::engrave));
    g.drawFittedText(button.getButtonText(), button.getLocalBounds().reduced(4, 0),
                     juce::Justification::centred, 1);
}

juce::Font OstraTornLookAndFeel::getLabelFont(juce::Label& label) {
    // A label whose parent is a slider is that slider's value read-out.
    const bool isValue = dynamic_cast<juce::Slider*>(label.getParentComponent()) != nullptr;
    const bool accentText = label.getProperties()[accentTextProperty];
    if (accentText) return boldFont(12.0f);
    return isValue ? labelFont(11.0f) : controlNameFont(9.5f);
}

juce::Font OstraTornLookAndFeel::getComboBoxFont(juce::ComboBox&) { return labelFont(11.0f); }

juce::Font OstraTornLookAndFeel::getPopupMenuFont() { return labelFont(13.0f); }

juce::Font OstraTornLookAndFeel::getTextButtonFont(juce::TextButton& button, int buttonHeight) {
    const bool accentText = button.getProperties()[accentTextProperty];
    const float size = juce::jlimit(9.0f, 15.0f, static_cast<float>(buttonHeight) * (accentText ? 0.52f : 0.46f));
    return accentText ? boldFont(size) : engravedFont(size);
}
