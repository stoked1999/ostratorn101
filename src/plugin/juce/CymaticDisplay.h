// ÖstraTorn101 — the cymatic display.
//
// The instrument's eye: the glowing ripple figure a sound makes on a water
// plate, drawn in the panel's amber.  The figure is a standing wave on a disc —
// its nodal lines are the filaments — and it is played by the instrument:
//
//   * the note being played sets the petal count (a driven plate gains nodes as
//     the frequency rises), so low notes are broad and high notes filigree,
//   * the brightness of the sound sets how many concentric rings the figure has,
//   * the envelope drives the glow, and
//   * every note strike sends a ripple out through the pattern, the way a struck
//     plate does.
//
// Cost: the audio thread only copies the block it has already rendered into a
// lock-free ring (see PluginProcessor::writeScope).  Everything else happens
// here, on the editor's own 30 Hz timer: analyse the ring, move the figure,
// render a small field image and scale it up.
#pragma once

#include <juce_gui_basics/juce_gui_basics.h>

#include <array>

#include "OstraTornLookAndFeel.h"
#include "PluginProcessor.h"

class CymaticDisplay : public juce::Component,
                       public juce::SettableTooltipClient,
                       private juce::Timer {
public:
    explicit CymaticDisplay(OstraTornAudioProcessor& processor);
    ~CymaticDisplay() override;

    void paint(juce::Graphics& g) override;
    void resized() override;

    // One analysis + animation step.  Driven by this component's own timer; the
    // tests call it directly.
    void tick();

    // What the display is showing right now (the tests read these).
    float level() const { return level_; }
    int   note() const { return note_; }            // detected pitch, -1 while silent
    int   petals() const { return petals_; }
    int   rings() const { return rings_; }
    int   strikeCount() const { return strikes_; }
    const juce::Image& fieldImageForTest() const { return fieldImage_; }   // diagnostics

private:
    void timerCallback() override { tick(); }

    void analyse();
    void updateModeTables();
    void renderField();
    // (Re)builds the field image and the per-cell geometry for the current size.
    void rebuildField();

    static constexpr int kWindowSamples = 4096;    // analysis window
    // The field is rendered at the disc's own size: filaments are a couple of
    // pixels wide, so rendering small and scaling up would average them away
    // into a dim wash.
    static constexpr int kFieldMaxSize = 240;
    static constexpr int kRadialSamples = 64;
    static constexpr int kThetaSamples = 360;

    OstraTornAudioProcessor& processor_;

    std::array<float, kWindowSamples> window_{};

    // Per-cell geometry: the field never moves, only its contents do, so which
    // radial and angular table entry each cell reads is computed once.  A radius
    // index of 255 marks cells outside the plate.
    int fieldSize_ = 96;
    std::array<juce::uint8, kFieldMaxSize * kFieldMaxSize> radiusIndex_{};
    std::array<juce::uint16, kFieldMaxSize * kFieldMaxSize> thetaIndex_{};
    std::array<float, kFieldMaxSize * kFieldMaxSize> grain_{};   // caustic grain per cell
    std::array<float, kFieldMaxSize * kFieldMaxSize> field_{};   // |standing wave| per cell

    std::array<float, kRadialSamples> radial1_{};   // J_n(k r), the main mode
    std::array<float, kRadialSamples> radial2_{};   // a second mode, for filigree
    std::array<float, kRadialSamples> radial3_{};   // J_0, the centre
    std::array<float, kThetaSamples> theta1_{};     // cos(n1 theta + phase)
    std::array<float, kThetaSamples> theta2_{};     // cos(-n2 theta + offset)

    juce::Image fieldImage_;

    // What the instrument is doing.
    float level_ = 0.0f;            // smoothed glow, 0..1
    float levelTarget_ = 0.0f;
    int   note_ = -1;
    int   strikes_ = 0;             // note strikes since the instance was created
    bool  gateOn_ = false;
    float phase_ = 0.0f;            // the figure's slow swirl

    // The figure, and where it is heading (it settles one node at a time).
    int   petals_ = 6;
    int   petalsTarget_ = 6;
    int   rings_ = 3;
    int   ringsTarget_ = 3;

    // The last note strike's ripple.
    float strikeAge_ = 10.0f;       // seconds since the strike
    float strikeAmplitude_ = 0.0f;  // rings down to zero

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(CymaticDisplay)
};
