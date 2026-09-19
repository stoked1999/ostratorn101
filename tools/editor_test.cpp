// ÖstraTorn101 — panel and preset-library smoke test.
//
// Why this exists: the VST3 hosting test (tools/vst3_host_test.cpp) can prove
// that the plugin loads, reports its parameters and makes sound, but it cannot
// inspect the editor — a host puts a plugin's view in a native child window, so
// the component the host holds has no JUCE children to look at.  That is exactly
// how an editor that never laid its controls out (a blank window) went unnoticed.
//
// This program builds the plugin's *own* editor, snapshots it and checks that the
// panel really draws, then exercises the preset library against the real user
// preset directory (saving, listing, loading back, deleting).
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

int countPixels(const juce::Image& image, int stride, std::function<bool(juce::Colour)> predicate) {
    int count = 0;
    for (int y = 0; y < image.getHeight(); y += stride) {
        for (int x = 0; x < image.getWidth(); x += stride) {
            if (predicate(image.getPixelAt(x, y))) ++count;
        }
    }
    return count;
}

} // namespace

int main(int argc, char** argv) {
    juce::ScopedJuceInitialiser_GUI juceInit;
    const juce::File outputPng = (argc > 1) ? juce::File(juce::String(argv[1]))
                                            : juce::File::getCurrentWorkingDirectory()
                                                  .getChildFile("plugin_ui.png");

    OstraTornAudioProcessor processor;
    processor.prepareToPlay(48000.0, 512);

    // ---- The factory bank is the plugin's program list ----------------------
    check(processor.getNumPrograms() == sh101::numPresets(),
          "the preset bank is exposed as the plugin's programs ("
              + juce::String(sh101::numPresets()) + " programs)");
    check(processor.getProgramName(2) == "Bass: Acid Bass", "program names are labelled");
    check(processor.getCurrentProgram() == 0, "a fresh instance starts on the reference patch");

    // ---- Loading a program moves every control ------------------------------
    const int acidBass = sh101::findPresetByName("Acid Bass");
    processor.setCurrentProgram(acidBass);
    const sh101::SH101Params expected = sh101::makePresetParams(acidBass);

    int mismatches = 0;
    for (int i = 0; i < sh101::kNumParams; ++i) {
        auto* parameter = processor.parameters().getParameter(OstraTornAudioProcessor::parameterId(i));
        if (parameter == nullptr) { ++mismatches; continue; }
        sh101::SH101Params probe = expected;
        sh101::applyNormalized(probe, i, parameter->getValue());
        if (std::fabs(sh101::getNormalized(probe, i) - sh101::getNormalized(expected, i)) > 1.0e-6) {
            std::printf("  mismatch on '%s'\n", sh101::paramName(i));
            ++mismatches;
        }
    }
    check(mismatches == 0, "every control matches the selected preset");
    check(! processor.presetIsModified(), "a freshly loaded preset is not marked modified");

    // ---- A sequenced preset delivers its pattern ---------------------------
    const int acidSequence = sh101::findPresetByName("Acid Sequence");
    processor.setCurrentProgram(acidSequence);
    {
        juce::AudioBuffer<float> buffer(2, 512);
        juce::MidiBuffer midi;
        midi.addEvent(juce::MidiMessage::noteOn(1, 48, 1.0f), 0);
        processor.processBlock(buffer, midi);
    }
    auto& sequencer = processor.adapter().engine().sequencer();
    check(sequencer.length() == 8, "the preset's sequence pattern reached the engine");
    const int expectedNotes[8] = { 36, 36, 48, 39, 36, 51, 48, 43 };
    int patternErrors = 0;
    for (int i = 0; i < 8; ++i) {
        if (sequencer.step(i).note != expectedNotes[i]) ++patternErrors;
    }
    check(patternErrors == 0, "the sequence holds the preset's notes");

    // ---- Audio, the level meter and standby --------------------------------
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

    {
        // The meter reads the last block, so trigger a fresh note first.
        juce::AudioBuffer<float> b(2, 512);
        juce::MidiBuffer m;
        m.addEvent(juce::MidiMessage::noteOn(1, 60, 1.0f), 0);
        processor.processBlock(b, m);
        std::printf("  level meter reads %.4f for a fresh note\n",
                    static_cast<double>(processor.outputLevel()));
        check(processor.outputLevel() > 0.01f, "the level meter sees the output");
    }
    check(! processor.isStandby(), "the instrument is powered by default");

    {
        processor.setStandby(true);
        juce::AudioBuffer<float> b(2, 512);
        juce::MidiBuffer m;
        m.addEvent(juce::MidiMessage::noteOn(1, 48, 1.0f), 0);
        processor.processBlock(b, m);
        check(b.getMagnitude(0, b.getNumSamples()) < 1.0e-9f, "standby silences the output");
        check(processor.outputLevel() == 0.0f, "standby drops the level meter to zero");
        processor.setStandby(false);
    }

    // ---- The panel's switch positions must reach the DSP -------------------
    // JUCE's raw parameter value for a choice parameter is the item *index*, not
    // the normalized position the engine's taper expects.  These checks go all the
    // way through to the parameters the engine is using, which is what a wrong
    // conversion would break.
    {
        const int chicago = sh101::findPresetByName("Chicago Bass");   // square LFO, 8' range
        processor.setCurrentProgram(chicago);
        juce::AudioBuffer<float> b(2, 256);
        juce::MidiBuffer m;
        processor.processBlock(b, m);

        const sh101::SH101Params engineParams = processor.adapter().params();
        const sh101::SH101Params expectedParams = sh101::makePresetParams(chicago);
        std::printf("  engine sees: lfoWave %d (want %d), vcoRange %d (want %d), subMode %d (want %d), "
                    "cutoff %.1f Hz (want %.1f)\n",
                    engineParams.lfoWave, expectedParams.lfoWave, engineParams.vcoRange,
                    expectedParams.vcoRange, engineParams.subMode, expectedParams.subMode,
                    engineParams.cutoff, expectedParams.cutoff);
        check(engineParams.lfoWave == expectedParams.lfoWave,
              "a switch position reaches the engine (LFO wave)");
        check(engineParams.vcoRange == expectedParams.vcoRange,
              "a switch position reaches the engine (VCO range)");
        check(engineParams.subMode == expectedParams.subMode,
              "a switch position reaches the engine (sub mode)");
        check(engineParams.arpMode == expectedParams.arpMode && engineParams.envTrigger == expectedParams.envTrigger
                  && engineParams.vcaMode == expectedParams.vcaMode,
              "the remaining switch positions agree at the engine");
        check(std::fabs(engineParams.cutoff - expectedParams.cutoff)
                  < 0.01 * expectedParams.cutoff,
              "a continuous control reaches the engine");
    }

    // ---- The user preset library (real files, real directory) --------------
    {
        const juce::String presetName = "Editor Test Patch";
        processor.setCurrentProgram(sh101::findPresetByName("Squelch Line"));
        // Change something so the saved document is not just the factory patch,
        // and so the "modified" marker has something to report.
        if (auto* cutoff = processor.parameters().getParameter(
                OstraTornAudioProcessor::parameterId(sh101::pCutoff))) {
            cutoff->setValueNotifyingHost(0.42f);
        }
        check(processor.presetIsModified(), "editing a control marks the preset modified");

        check(processor.saveUserPreset(presetName), "a user preset is saved to the library");
        const juce::File file = processor.userPresetFile(presetName);
        std::printf("  user preset file: %s (%lld bytes)\n", file.getFullPathName().toRawUTF8(),
                    static_cast<long long>(file.getSize()));
        check(file.existsAsFile(), "the preset file exists on disk");
        check(file.loadFileAsString().contains("name=" + presetName), "the file carries the name");
        check(processor.userPresetNames().contains(presetName), "the library lists the new preset");

        const float savedCutoff = processor.parameters()
                                      .getParameter(OstraTornAudioProcessor::parameterId(sh101::pCutoff))
                                      ->getValue();

        // Load something else, then come back: the saved values must return.
        processor.setCurrentProgram(0);
        check(processor.loadUserPreset(presetName), "the user preset loads back");
        const float reloadedCutoff =
            processor.parameters().getParameter(OstraTornAudioProcessor::parameterId(sh101::pCutoff))
                ->getValue();
        std::printf("  cutoff before %.6f, after reload %.6f\n", savedCutoff, reloadedCutoff);
        check(std::fabs(savedCutoff - reloadedCutoff) < 1.0e-6f, "the saved sound comes back exactly");
        check(processor.currentPresetIsUser(), "the display knows this is a user preset");
        check(processor.currentPresetName() == presetName, "the display knows the preset's name");
        check(! processor.presetIsModified(), "a reloaded preset is not marked modified");

        check(processor.deleteUserPreset(presetName), "the user preset can be deleted");
        check(! file.existsAsFile(), "the file is gone after deletion");
    }

    // ---- The editor draws its panel ----------------------------------------
    std::unique_ptr<juce::AudioProcessorEditor> editor(processor.createEditor());
    check(editor != nullptr, "the plugin creates an editor");
    if (editor != nullptr) {
        processor.setCurrentProgram(acidBass);
        std::printf("  editor size: %d x %d, direct children: %d\n", editor->getWidth(),
                    editor->getHeight(), editor->getNumChildComponents());
        check(editor->getWidth() > 900 && editor->getHeight() > 500, "the editor has a usable size");
        check(editor->getNumChildComponents() >= 60, "the editor owns its controls");

        int zeroSized = 0;
        for (int i = 0; i < editor->getNumChildComponents(); ++i) {
            const auto* child = editor->getChildComponent(i);
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

        int sliders = 0;
        int slidersVisible = 0;
        for (int i = 0; i < editor->getNumChildComponents(); ++i) {
            if (auto* s = dynamic_cast<juce::Slider*>(editor->getChildComponent(i))) {
                ++sliders;
                if (s->isVisible()) ++slidersVisible;
                if (sliders == 1) {
                    const juce::String readOut = s->getTextFromValue(s->getValue());
                    std::printf("  first fader: bounds %s, read-out '%s'\n",
                                s->getBounds().toString().toRawUTF8(), readOut.toRawUTF8());
                    check(readOut.endsWith("Hz") || readOut.endsWith("%") || readOut.endsWith("ms")
                              || readOut.endsWith("s") || readOut.endsWith("ct")
                              || readOut.endsWith("oct") || readOut.endsWith("st")
                              || readOut.endsWith("kHz"),
                          "a fader's value is shown in engineering units, not as a raw 0..1 number");
                    const auto sliderImage = s->createComponentSnapshot(s->getLocalBounds(), false);
                    const int ink = countPixels(sliderImage, 1, [](juce::Colour c) {
                        return c.getAlpha() > 0 && c.getBrightness() > 0.12f;
                    });
                    std::printf("  first fader paints %d pixel(s)\n", ink);
                    check(ink > 100, "the first fader actually paints");
                }
            }
        }
        std::printf("  sliders: %d (visible %d)\n", sliders, slidersVisible);
        check(sliders > 20, "the panel has its fader controls");

        editor->setVisible(true);
        const juce::Image snapshot = editor->createComponentSnapshot(editor->getLocalBounds(), false);
        const int sampledPixels = (snapshot.getWidth() / 2) * (snapshot.getHeight() / 2);

        // Two colours define the panel: the brushed body (light grey) and the
        // amber of the fader bands and drop-down arrows.
        const int bodyPixels = countPixels(snapshot, 2, [](juce::Colour c) {
            return c.getRed() > 150 && c.getRed() < 245 && std::abs(c.getRed() - c.getBlue()) < 14;
        });
        const int accentPixels = countPixels(snapshot, 2, [](juce::Colour c) {
            return c.getRed() > 200 && c.getGreen() > 100 && c.getGreen() < 190 && c.getBlue() < 120;
        });
        const double bodyFraction = static_cast<double>(bodyPixels) / static_cast<double>(sampledPixels);
        std::printf("  panel: %.1f%% metal, %d accent pixels, %d sampled\n", bodyFraction * 100.0,
                    accentPixels, sampledPixels);
        check(bodyFraction > 0.35, "the panel is drawn as a metal plate");
        check(accentPixels > 500, "the accent colour is present (fader bands, LEDs, arrows)");

        // The amber accents must appear in every band of the control area, which
        // proves the sections are spread down the panel rather than bunched.
        constexpr int kControlsTop = 192;   // margin + title bar + preset bar
        constexpr int kBands = 4;
        const int bandHeight = (snapshot.getHeight() - kControlsTop) / kBands;
        int bandsWithAccent = 0;
        for (int band = 0; band < kBands; ++band) {
            const int y0 = kControlsTop + band * bandHeight;
            const int y1 = kControlsTop + (band + 1) * bandHeight;
            int inBand = 0;
            for (int y = y0; y < y1; y += 2) {
                for (int x = 0; x < snapshot.getWidth(); x += 2) {
                    const auto c = snapshot.getPixelAt(x, y);
                    if (c.getRed() > 200 && c.getGreen() > 100 && c.getGreen() < 190 && c.getBlue() < 120) {
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
            check(png.writeImageToStream(snapshot, *stream),
                  "the panel snapshot was written to " + outputPng.getFullPathName());
        } else {
            check(false, "could not open " + outputPng.getFullPathName() + " for writing");
        }
    }

    std::printf("%s: %d failure(s)\n", gFailures == 0 ? "PASS" : "FAIL", gFailures);
    return gFailures == 0 ? 0 : 1;
}
