// Editor + preset smoke test for the JUCE wrapper.
//
// Why this exists: the VST3 hosting test (tools/vst3_host_test.cpp) can prove
// that the plugin loads, reports its parameters and makes sound, but it cannot
// inspect the editor — a host puts a plugin's view in a native child window, so
// the component the host holds has no JUCE children to look at.  That is exactly
// how an editor that never laid its controls out (a blank window) went
// unnoticed.  This program builds the plugin's *own* editor directly, snapshots
// it and checks that the panel really draws, so a blank window fails the test.
//
// Usage: sh101_editor_test [output.png]
#include <cmath>
#include <cstdio>
#include <memory>

#include <juce_gui_extra/juce_gui_extra.h>

#include "PluginEditor.h"
#include "PluginProcessor.h"
#include "sh101/Params.h"
#include "sh101/Presets.h"

namespace {

int gFailures = 0;

void check(bool ok, const juce::String& what) {
    if (!ok) {
        std::printf("FAIL: %s\n", what.toRawUTF8());
        ++gFailures;
    } else {
        std::printf("  ok: %s\n", what.toRawUTF8());
    }
}

} // namespace

int main(int argc, char** argv) {
    juce::ScopedJuceInitialiser_GUI juceInit;
    const juce::File outputPng = (argc > 1) ? juce::File(juce::String(argv[1]))
                                            : juce::File::getCurrentWorkingDirectory()
                                                  .getChildFile("plugin_ui.png");

    SH101AudioProcessor processor;
    processor.prepareToPlay(48000.0, 512);

    // ---- Presets are the plugin's programs ---------------------------------
    check(processor.getNumPrograms() == sh101::numPresets(),
          "the preset bank is exposed as the plugin's programs");
    check(processor.getProgramName(2) == "Bass: Acid Bass", "program names are labelled");
    check(processor.getCurrentProgram() == 0, "a fresh instance starts on the reference patch");

    // ---- Loading a program moves every control ------------------------------
    const int acidBass = sh101::findPresetByName("Acid Bass");
    processor.setCurrentProgram(acidBass);
    const sh101::SH101Params expected = sh101::makePresetParams(acidBass);

    int mismatches = 0;
    for (int i = 0; i < sh101::kNumParams; ++i) {
        auto* parameter = processor.parameters().getParameter(SH101AudioProcessor::parameterId(i));
        if (parameter == nullptr) { ++mismatches; continue; }
        // Convert the host's value back the way the engine does and compare.
        sh101::SH101Params probe = expected;
        sh101::applyNormalized(probe, i, parameter->getValue());
        if (std::fabs(sh101::getNormalized(probe, i) - sh101::getNormalized(expected, i)) > 1.0e-6) {
            std::printf("  mismatch on '%s'\n", sh101::paramName(i));
            ++mismatches;
        }
    }
    check(mismatches == 0, "every control matches the selected preset");
    check(processor.getCurrentProgram() == acidBass, "the current program follows the selection");

    // ---- A sequenced preset delivers its pattern and then plays -------------
    const int acidSequence = sh101::findPresetByName("Acid Sequence");
    processor.setCurrentProgram(acidSequence);

    juce::AudioBuffer<float> buffer(2, 512);
    juce::MidiBuffer midi;
    midi.addEvent(juce::MidiMessage::noteOn(1, 48, 1.0f), 0);
    processor.processBlock(buffer, midi);

    auto& sequencer = processor.adapter().engine().sequencer();
    check(sequencer.length() == 8, "the preset's sequence pattern reached the engine");
    const int expectedNotes[8] = { 36, 36, 48, 39, 36, 51, 48, 43 };
    int patternErrors = 0;
    for (int i = 0; i < 8; ++i) {
        if (sequencer.step(i).note != expectedNotes[i]) ++patternErrors;
    }
    check(patternErrors == 0, "the sequence holds the preset's notes");

    double peak = 0.0;
    for (int block = 0; block < 60; ++block) {
        juce::AudioBuffer<float> b(2, 512);
        juce::MidiBuffer m;
        if (block == 1) m.addEvent(juce::MidiMessage::noteOn(1, 48, 1.0f), 0);
        processor.processBlock(b, m);
        peak = std::max(peak, static_cast<double>(b.getMagnitude(0, b.getNumSamples())));
    }
    std::printf("  rendered peak after a preset load: %.4f\n", peak);
    check(peak > 0.01, "a loaded preset makes sound in the plugin");

    // ---- The editor draws its panel ----------------------------------------
    std::unique_ptr<juce::AudioProcessorEditor> editor(processor.createEditor());
    check(editor != nullptr, "the plugin creates an editor");
    if (editor != nullptr) {
        std::printf("  editor size: %d x %d, direct children: %d\n", editor->getWidth(),
                    editor->getHeight(), editor->getNumChildComponents());
        check(editor->getWidth() > 400 && editor->getHeight() > 400, "the editor has a usable size");
        // 34 controls + 34 name labels, plus the preset widgets.  A layout that
        // never ran would still have the children — but at zero size, which the
        // snapshot check below catches.
        check(editor->getNumChildComponents() >= 60, "the editor owns its controls");

        int zeroSized = 0;
        for (int i = 0; i < editor->getNumChildComponents(); ++i) {
            const auto* child = editor->getChildComponent(i);
            // The tooltip window is a desktop helper, not a laid-out control.
            if (dynamic_cast<const juce::TooltipWindow*>(child) != nullptr) continue;
            if (child->getWidth() <= 0 || child->getHeight() <= 0) {
                std::printf("  zero-sized child: %s\n",
                            (dynamic_cast<const juce::Slider*>(child) != nullptr) ? "slider"
                            : (dynamic_cast<const juce::ComboBox*>(child) != nullptr) ? "combo"
                            : (dynamic_cast<const juce::Label*>(child) != nullptr) ? "label"
                            : (dynamic_cast<const juce::Button*>(child) != nullptr) ? "button"
                                                                                   : "other");
                ++zeroSized;
            }
        }
        check(zeroSized == 0, "every control was laid out (the blank-window bug)");

        // Diagnostics for the fader controls: a slider that is present and laid
        // out but paints nothing is still a broken panel.
        int sliders = 0;
        int slidersVisible = 0;
        for (int i = 0; i < editor->getNumChildComponents(); ++i) {
            if (auto* s = dynamic_cast<juce::Slider*>(editor->getChildComponent(i))) {
                ++sliders;
                if (s->isVisible()) ++slidersVisible;
                if (sliders == 1) {
                    std::printf("  first slider: bounds %s, visible=%d, style=%d, textBox=%d\n",
                                s->getBounds().toString().toRawUTF8(), (int) s->isVisible(),
                                (int) s->getSliderStyle(), (int) s->getTextBoxPosition());
                    // The read-out text must come from the panel's formatter, not
                    // from JUCE's raw-number fallback.
                    const juce::String readOut = s->getTextFromValue(s->getValue());
                    std::printf("  first slider read-out: '%s' (formatter %s)\n", readOut.toRawUTF8(),
                                s->textFromValueFunction != nullptr ? "set" : "NOT SET");
                    check(readOut.endsWith("Hz") || readOut.endsWith("%") || readOut.endsWith("ms")
                              || readOut.endsWith("s") || readOut.endsWith("ct")
                              || readOut.endsWith("oct") || readOut.endsWith("st")
                              || readOut.endsWith("k"),
                          "a fader's value is shown in engineering units, not as a raw 0..1 number");

                    const auto sliderImage = s->createComponentSnapshot(s->getLocalBounds(), false);
                    int inkPixels = 0;
                    for (int y = 0; y < sliderImage.getHeight(); ++y) {
                        for (int x = 0; x < sliderImage.getWidth(); ++x) {
                            const auto c = sliderImage.getPixelAt(x, y);
                            if (c.getAlpha() > 0 && c.getBrightness() > 0.12f) ++inkPixels;
                        }
                    }
                    std::printf("  first slider paints %d pixel(s) below the background\n", inkPixels);
                    check(inkPixels > 100, "the first fader actually paints");
                }
            }
        }
        std::printf("  sliders: %d, visible: %d\n", sliders, slidersVisible);
        check(sliders > 20, "the panel has its fader controls");

        // Isolated check: can the look-and-feel draw a fader on its own?  This
        // separates "the look-and-feel cannot draw" from "the editor wired the
        // slider up wrongly".
        {
            SH101LookAndFeel probeLook;
            juce::Slider probe(juce::Slider::LinearVertical, juce::Slider::TextBoxBelow);
            probe.setLookAndFeel(&probeLook);
            probe.setRange(0.0, 1.0, 0.0);
            probe.setTextBoxStyle(juce::Slider::TextBoxBelow, true, 0, 15);
            probe.setBounds(0, 0, 60, 56);
            probe.setValue(0.5);
            probe.setVisible(true);
            probe.textFromValueFunction = [](double) { return juce::String("PROBE"); };
            std::printf("  probe hook: formatter %s, getTextFromValue -> '%s'\n",
                        probe.textFromValueFunction != nullptr ? "set" : "NOT SET",
                        probe.getTextFromValue(0.5).toRawUTF8());

            const auto layout = probeLook.getSliderLayout(probe);
            std::printf("  probe layout: sliderBounds %s, textBox %s\n",
                        layout.sliderBounds.toString().toRawUTF8(),
                        layout.textBoxBounds.toString().toRawUTF8());

            const auto probeImage = probe.createComponentSnapshot(probe.getLocalBounds(), false);
            int ink = 0;
            int amber = 0;
            for (int y = 0; y < probeImage.getHeight(); ++y) {
                for (int x = 0; x < probeImage.getWidth(); ++x) {
                    const auto c = probeImage.getPixelAt(x, y);
                    if (c.getAlpha() > 0 && c.getBrightness() > 0.12f) ++ink;
                    if (c.getRed() > 200 && c.getGreen() > 110 && c.getGreen() < 215 && c.getBlue() < 130) {
                        ++amber;
                    }
                }
            }
            std::printf("  probe fader: %dx%d image, %d bright pixel(s), %d amber\n",
                        probeImage.getWidth(), probeImage.getHeight(), ink, amber);
            check(amber > 100, "the look-and-feel draws a fader cap on its own");
            probe.setLookAndFeel(nullptr);
        }

        editor->setVisible(true);
        const juce::Image snapshot = editor->createComponentSnapshot(editor->getLocalBounds(), false);
        const int totalPixels = snapshot.getWidth() * snapshot.getHeight();

        int drawn = 0;         // pixels that differ from the window background
        int accentPixels = 0;  // the amber used for fader caps, arrows and headers
        for (int y = 0; y < snapshot.getHeight(); y += 2) {
            for (int x = 0; x < snapshot.getWidth(); x += 2) {
                const juce::Colour c = snapshot.getPixelAt(x, y);
                if (std::abs(c.getRed() - 0x15) + std::abs(c.getGreen() - 0x16) + std::abs(c.getBlue() - 0x1a) > 18) {
                    ++drawn;
                }
                if (c.getRed() > 200 && c.getGreen() > 110 && c.getGreen() < 215 && c.getBlue() < 130) {
                    ++accentPixels;
                }
            }
        }
        const double drawnFraction = static_cast<double>(drawn) / static_cast<double>(totalPixels / 4);
        std::printf("  drawn pixels: %.1f%% of the window, accent pixels: %d\n", drawnFraction * 100.0,
                    accentPixels);
        check(drawnFraction > 0.10, "the panel paints a substantial part of the window");
        check(accentPixels > 200, "the panel's accent colour is present (fader caps, headers)");

        // Accent pixels must appear in every band of the control area (below the
        // top bar): that proves the sections are spread down the panel rather
        // than bunched in one place.
        constexpr int kTopBarHeight = 92;
        constexpr int kBands = 4;
        const int bandHeight = (snapshot.getHeight() - kTopBarHeight) / kBands;
        int bandsWithAccent = 0;
        for (int band = 0; band < kBands; ++band) {
            const int y0 = kTopBarHeight + band * bandHeight;
            const int y1 = kTopBarHeight + (band + 1) * bandHeight;
            int inBand = 0;
            for (int y = y0; y < y1; y += 2) {
                for (int x = 0; x < snapshot.getWidth(); x += 2) {
                    const juce::Colour c = snapshot.getPixelAt(x, y);
                    if (c.getRed() > 200 && c.getGreen() > 110 && c.getGreen() < 215 && c.getBlue() < 130) {
                        ++inBand;
                    }
                }
            }
            if (inBand > 10) ++bandsWithAccent;
        }
        std::printf("  control-area bands containing accents: %d of %d\n", bandsWithAccent, kBands);
        check(bandsWithAccent == kBands, "controls and headers are spread across the whole panel");

        juce::PNGImageFormat png;
        outputPng.deleteFile();
        if (auto stream = std::unique_ptr<juce::FileOutputStream>(outputPng.createOutputStream())) {
            const bool wrote = png.writeImageToStream(snapshot, *stream);
            check(wrote, "the editor snapshot was written to " + outputPng.getFullPathName());
        } else {
            check(false, "could not open " + outputPng.getFullPathName() + " for writing");
        }
    }

    std::printf("%s: %d failure(s)\n", gFailures == 0 ? "PASS" : "FAIL", gFailures);
    return gFailures == 0 ? 0 : 1;
}
