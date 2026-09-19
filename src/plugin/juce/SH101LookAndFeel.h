// Panel look-and-feel for the SH-101 editor.
//
// The aim is the visual language of the instrument rather than a copy of its
// artwork: a dark panel, one column of fader-style controls per section, amber
// accents for anything you can grab, cream text for labels.  No manufacturer
// logos, model artwork or brand fonts are used (brief: behaviour and sound, not
// branding).
#pragma once

#include <juce_gui_basics/juce_gui_basics.h>

namespace sh101ui {

struct Palette {
    static const juce::Colour windowBg;      // editor background
    static const juce::Colour panelBg;       // section panel
    static const juce::Colour panelOutline;  // panel border
    static const juce::Colour headerText;    // section titles
    static const juce::Colour text;          // control labels and values
    static const juce::Colour textDim;       // secondary text
    static const juce::Colour accent;        // fader caps, arrows, highlight
    static const juce::Colour accentDim;
    static const juce::Colour trackBg;       // fader groove / combo background
    static const juce::Colour trackFill;     // filled part of a fader
    static const juce::Colour buttonBg;
};

// Fonts are defined in one place so the panel stays typographically consistent.
inline juce::Font panelFont(float height) {
    return juce::Font(juce::FontOptions(height));
}
inline juce::Font panelFontBold(float height) {
    return juce::Font(juce::FontOptions(height, juce::Font::bold));
}

} // namespace sh101ui

class SH101LookAndFeel : public juce::LookAndFeel_V4 {
public:
    // Height of the value read-out strip drawn under each fader.
    static constexpr int kReadoutHeight = 15;

    SH101LookAndFeel();

    void drawLinearSlider(juce::Graphics& g, int x, int y, int width, int height,
                          float sliderPos, float minSliderPos, float maxSliderPos,
                          juce::Slider::SliderStyle style, juce::Slider& slider) override;

    void drawComboBox(juce::Graphics& g, int width, int height, bool isButtonDown, int buttonX,
                      int buttonY, int buttonW, int buttonH, juce::ComboBox& box) override;
    void positionComboBoxText(juce::ComboBox& box, juce::Label& label) override;

    // The inherited heuristic hands a short vertical fader only a sliver of its
    // height (and a zero-width value read-out), which leaves nothing to draw.
    // The fader layout is therefore defined here: a full-height groove with a
    // fixed-height read-out underneath.
    juce::Slider::SliderLayout getSliderLayout(juce::Slider& slider) override;

    void drawButtonBackground(juce::Graphics& g, juce::Button& button,
                              const juce::Colour& backgroundColour, bool shouldDrawButtonAsHighlighted,
                              bool shouldDrawButtonAsDown) override;

    juce::Font getLabelFont(juce::Label& label) override;
    juce::Font getComboBoxFont(juce::ComboBox& box) override;
    juce::Font getPopupMenuFont() override;
    juce::Font getTextButtonFont(juce::TextButton& button, int buttonHeight) override;

    // Draws a fader's value in engineering units.  Drawn here rather than by a
    // Slider text box so the read-out always uses the engine's own taper.
    void drawSliderReadOut(juce::Graphics& g, juce::Rectangle<int> area, juce::Slider& slider,
                           bool highlighted);
};
