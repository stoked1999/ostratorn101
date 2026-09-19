// ÖstraTorn101 — panel look-and-feel.
//
// The instrument is drawn as hardware: a brushed-metal body with recessed section
// panels, a dark tinted window for the preset display, chunky faders with orange
// caps, drop-downs with an orange arrow, indicator LEDs and panel screws.  No
// manufacturer logos or product artwork are reproduced; the panel says what it is
// ("ANALOGUE MONO SYNTHESIZER", "INSPIRED BY SH-101").
#pragma once

#include <juce_gui_basics/juce_gui_basics.h>

namespace ostra {

// ---- Panel colours ----------------------------------------------------------
struct Palette {
    static const juce::Colour bodyTop;        // brushed body, top of the gradient
    static const juce::Colour bodyBottom;
    static const juce::Colour panelTop;       // raised section panel
    static const juce::Colour panelBottom;
    static const juce::Colour bevelLight;     // top/left highlight of a bevel
    static const juce::Colour bevelDark;      // bottom/right shadow of a bevel
    static const juce::Colour engrave;        // panel lettering
    static const juce::Colour engraveSoft;    // secondary lettering
    static const juce::Colour displayTop;     // tinted window
    static const juce::Colour displayBottom;
    static const juce::Colour displayText;
    static const juce::Colour accent;         // the orange of the fader bands
    static const juce::Colour accentDark;
    static const juce::Colour slotDark;       // fader slot
    static const juce::Colour capTop;         // fader cap
    static const juce::Colour capBottom;
    static const juce::Colour ledRed;
    static const juce::Colour ledAmber;
    static const juce::Colour ledYellow;
    static const juce::Colour ledGreen;
    static const juce::Colour screw;
};

// Property set on a button/label to ask for accent-coloured text.
inline const juce::Identifier accentTextProperty{ "accentText" };

// ---- Low-level hardware drawing ---------------------------------------------
void drawBrushedBody(juce::Graphics& g, juce::Rectangle<float> area);
void drawSectionPanel(juce::Graphics& g, juce::Rectangle<float> area, bool raised = true);
void drawTintedDisplay(juce::Graphics& g, juce::Rectangle<float> area);
void drawScrew(juce::Graphics& g, juce::Point<float> centre, float radius);
void drawLed(juce::Graphics& g, juce::Point<float> centre, float radius, juce::Colour colour,
             bool lit);

// Fonts in one place so the panel stays typographically consistent.
inline juce::Font labelFont(float height) { return juce::Font(juce::FontOptions(height)); }
inline juce::Font boldFont(float height) {
    return juce::Font(juce::FontOptions(height, juce::Font::bold));
}
// Panel lettering: small, bold, wide-tracked, like screen print on metal.
inline juce::Font engravedFont(float height) {
    return juce::Font(juce::FontOptions(height, juce::Font::bold)).withExtraKerningFactor(0.12f);
}
// Control names: smaller, so a 45-pixel-wide cell still fits "PWM SRC".
inline juce::Font controlNameFont(float height) {
    return juce::Font(juce::FontOptions(height, juce::Font::bold)).withExtraKerningFactor(0.04f);
}

} // namespace ostra

class OstraTornLookAndFeel : public juce::LookAndFeel_V4 {
public:
    // Height of the value read-out strip under each fader.
    static constexpr int kReadoutHeight = 15;

    OstraTornLookAndFeel();

    void drawLinearSlider(juce::Graphics& g, int x, int y, int width, int height, float sliderPos,
                          float minSliderPos, float maxSliderPos, juce::Slider::SliderStyle style,
                          juce::Slider& slider) override;

    void drawComboBox(juce::Graphics& g, int width, int height, bool isButtonDown, int buttonX,
                      int buttonY, int buttonW, int buttonH, juce::ComboBox& box) override;
    void positionComboBoxText(juce::ComboBox& box, juce::Label& label) override;

    // A short fader gets almost no groove from the inherited heuristic, so the
    // fader layout is defined here instead.
    juce::Slider::SliderLayout getSliderLayout(juce::Slider& slider) override;

    void drawButtonBackground(juce::Graphics& g, juce::Button& button,
                              const juce::Colour& backgroundColour,
                              bool shouldDrawButtonAsHighlighted, bool shouldDrawButtonAsDown) override;
    void drawButtonText(juce::Graphics& g, juce::TextButton& button,
                        bool shouldDrawButtonAsHighlighted, bool shouldDrawButtonAsDown) override;

    juce::Font getLabelFont(juce::Label& label) override;
    juce::Font getComboBoxFont(juce::ComboBox& box) override;
    juce::Font getPopupMenuFont() override;
    juce::Font getTextButtonFont(juce::TextButton& button, int buttonHeight) override;

    // Draws a fader's value in engineering units, under the groove.
    void drawSliderReadOut(juce::Graphics& g, juce::Rectangle<int> area, juce::Slider& slider,
                           bool highlighted);
};
