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

// A synthetic mouse event, for driving the step editor's own interaction code
// (the panel's slots are hit-tested in their component's coordinates).
juce::MouseEvent makeMouseEvent(juce::Component& component, juce::Point<float> position,
                                juce::ModifierKeys modifiers = {}, juce::Point<float> downPosition = {}) {
    const auto now = juce::Time::getCurrentTime();
    const auto down = downPosition.isOrigin() ? position : downPosition;
    return juce::MouseEvent(juce::Desktop::getInstance().getMainMouseSource(), position, modifiers,
                            1.0f, 0.0f, 0.0f, 0.0f, 0.0f, &component, &component, now, down, now, 1,
                            down != position);
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

    // ---- The step editor's editing API reaches the engine -------------------
    // Edits are made on the message thread against the processor's mirror and
    // published to the audio thread, so each check renders a block first.
    {
        const auto renderOne = [&processor] {
            juce::AudioBuffer<float> b(2, 512);
            juce::MidiBuffer m;
            processor.processBlock(b, m);
        };

        processor.setPatternStep(0, 62, true, false);
        renderOne();
        check(sequencer.step(0).note == 62, "an edited step reaches the engine");

        processor.setPatternStep(1, 43, false, false);
        renderOne();
        check(! sequencer.step(1).gate, "a rest reaches the engine");

        processor.setPatternStep(1, 43, true, true);
        renderOne();
        check(sequencer.step(1).gate && sequencer.step(1).tie, "a tie reaches the engine");

        // The sequence length is the seqLength host parameter.
        check(processor.patternLength() == 8, "the pattern length follows the seqLength parameter");
        processor.setPatternLength(4);
        renderOne();
        check(sequencer.length() == 4, "the seqLength parameter sets the played length");
        check(processor.patternLength() == 4, "and reads back through the panel's accessor");

        // The playhead: while the sequence runs the processor publishes the step
        // it is on, which is what the panel's playhead follows.
        int firstStep = processor.sequencePlayStep();
        bool advanced = false;
        for (int block = 0; block < 60; ++block) {
            renderOne();
            if (processor.sequencePlayStep() != firstStep) advanced = true;
        }
        check(processor.sequenceRunning(), "the sequence reports that it is running");
        check(advanced, "the playhead telemetry advances while the sequence runs");

        // Editing the pattern marks the preset modified; loading it again clears
        // that, the same way a control edit does.
        check(processor.presetIsModified(), "editing a step marks the preset modified");
        processor.setCurrentProgram(acidSequence);
        renderOne();
        check(! processor.presetIsModified(), "reloading the preset clears the modified marker");

        // A session save/restore carries the pattern.
        juce::MemoryBlock state;
        processor.getStateInformation(state);
        const int savedStep2 = processor.patternStep(2).note;
        processor.setPatternStep(2, 70, true, false);
        renderOne();
        check(sequencer.step(2).note == 70, "the edit is playing before the restore");
        processor.setStateInformation(state.getData(), static_cast<int>(state.getSize()));
        renderOne();
        check(processor.patternStep(2).note == savedStep2 && sequencer.step(2).note == savedStep2,
              "the pattern survives a session save and restore");

        // Back to the preset as loaded, for the checks that follow.
        processor.setCurrentProgram(acidSequence);
        renderOne();
    }

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

        // ---- The step editor component: slots, interaction and REC ----------
        auto* panel = dynamic_cast<OstraTornAudioProcessorEditor*>(editor.get());
        check(panel != nullptr, "the editor is the plugin's own panel");
        if (panel != nullptr) {
            StepEditor& steps = panel->stepEditor();
            std::printf("  step editor: %dx%d, %d slot(s), page %d of %d\n", steps.getWidth(),
                        steps.getHeight(), StepEditor::kSlots, steps.page() + 1, steps.pageCount());
            check(steps.getWidth() > 800 && steps.getHeight() > 120,
                  "the step editor has its own row on the panel");
            check(StepEditor::kSlots == 16, "16 step slots are shown at once");

            processor.setCurrentProgram(acidSequence);   // 8 programmed steps, seq on
            {
                juce::AudioBuffer<float> b(2, 512);
                juce::MidiBuffer m;
                processor.processBlock(b, m);
            }

            // Click a slot: a note becomes a rest, and clicking again brings it
            // back — the plainest programming gesture there is.
            const int slot = 4;                       // page 0, so step 4
            const juce::Point<float> slotCentre = steps.slotCentre(slot).toFloat();
            const bool gatedBefore = processor.patternStep(slot).gate;
            steps.mouseDown(makeMouseEvent(steps, slotCentre));
            steps.mouseUp(makeMouseEvent(steps, slotCentre));
            check(processor.patternStep(slot).gate != gatedBefore,
                  "clicking a step toggles it between note and rest");
            steps.mouseDown(makeMouseEvent(steps, slotCentre));
            steps.mouseUp(makeMouseEvent(steps, slotCentre));
            check(processor.patternStep(slot).gate == gatedBefore, "clicking again restores it");

            // Drag a slot: the pitch follows the pointer, and the edit reaches the
            // engine.
            const int noteBefore = processor.patternStep(2).note;
            {
                const juce::Point<float> start = steps.slotCentre(2).toFloat();
                const juce::Point<float> end(start.x, start.y - 30.0f);
                steps.mouseDown(makeMouseEvent(steps, start));
                steps.mouseDrag(makeMouseEvent(steps, end, {}, start));
                steps.mouseUp(makeMouseEvent(steps, end, {}, start));
            }
            const int noteAfter = processor.patternStep(2).note;
            check(noteAfter > noteBefore, "dragging a step up raises its pitch");
            check(processor.patternStep(2).gate, "a dragged step becomes a gated note");
            {
                juce::AudioBuffer<float> b(2, 512);
                juce::MidiBuffer m;
                processor.processBlock(b, m);
            }
            check(sequencer.step(2).note == noteAfter, "the dragged pitch reaches the engine");

            // Shift-click ties the step into the next one.
            const bool tieBefore = processor.patternStep(slot).tie;
            steps.mouseDown(makeMouseEvent(steps, slotCentre, juce::ModifierKeys::shiftModifier));
            check(processor.patternStep(slot).tie != tieBefore, "a shift-click toggles the tie");
            check(processor.patternStep(slot).gate, "a tied step is a note, not a rest");
            {
                juce::AudioBuffer<float> b(2, 512);
                juce::MidiBuffer m;
                processor.processBlock(b, m);
            }
            check(sequencer.step(slot).tie, "the tie reaches the engine");
            steps.mouseDown(makeMouseEvent(steps, slotCentre, juce::ModifierKeys::shiftModifier));
            check(processor.patternStep(slot).tie == tieBefore, "shift-clicking again clears the tie");

            // REC: a note played on the keyboard is written into the step and the
            // write head advances — how the instrument records.
            processor.setCurrentProgram(sh101::findPresetByName("Squelch Line"));   // sequencer off
            {
                juce::AudioBuffer<float> b(2, 512);
                juce::MidiBuffer m;
                processor.processBlock(b, m);
            }
            steps.setPage(0);
            steps.tick();        // the panel's timer drains continuously: flush first
            steps.setRecArmed(true);
            const int writeHead = steps.writeHead();
            check(writeHead == 0, "the write head starts at the first step");
            {
                juce::AudioBuffer<float> b(2, 512);
                juce::MidiBuffer m;
                m.addEvent(juce::MidiMessage::noteOn(1, 55, 1.0f), 0);
                m.addEvent(juce::MidiMessage::noteOff(1, 55), 200);
                processor.processBlock(b, m);
            }
            steps.tick();
            std::printf("  REC wrote into step %d, write head now %d\n", writeHead, steps.writeHead());
            check(processor.patternStep(writeHead).note == 55,
                  "REC writes a played note into the armed step");
            check(steps.writeHead() == (writeHead + 1) % processor.patternLength(),
                  "the write head advances after a recorded note");
            steps.setRecArmed(false);

            // The step editor's face paints (its slots are the panel's own amber).
            const auto stepImage =
                steps.createComponentSnapshot(steps.getLocalBounds(), false);
            const int stepAmber = countPixels(stepImage, 1, [](juce::Colour c) {
                return c.getRed() > 200 && c.getGreen() > 100 && c.getGreen() < 190 && c.getBlue() < 120;
            });
            std::printf("  step editor paints %d amber pixel(s)\n", stepAmber);
            check(stepAmber > 150, "the step editor draws its slots");

            // ---- The cymatic display: the sound made visible ----------------
            CymaticDisplay& cymatic = panel->cymaticDisplay();
            std::printf("  cymatic display: %dx%d\n", cymatic.getWidth(), cymatic.getHeight());
            check(cymatic.getWidth() > 120 && cymatic.getHeight() > 140,
                  "the cymatic display has its window on the panel");

            // Play a note into the plugin, then read the display.
            processor.setCurrentProgram(acidBass);
            for (int block = 0; block < 20; ++block) {
                juce::AudioBuffer<float> b(2, 512);
                juce::MidiBuffer m;
                if (block == 0) m.addEvent(juce::MidiMessage::noteOn(1, 60, 1.0f), 0);
                processor.processBlock(b, m);
            }
            {
                float scope[256] = {};
                const int got = processor.readScopeSamples(scope, 256);
                double energy = 0.0;
                for (int i = 0; i < got; ++i) energy += static_cast<double>(scope[i]) * scope[i];
                check(got == 256 && energy > 0.0, "the signal feed carries the rendered output");
            }

            for (int i = 0; i < 8; ++i) cymatic.tick();
            std::printf("  cymatic: level %.3f, note %d, petals %d, rings %d, strikes %d\n",
                        static_cast<double>(cymatic.level()), cymatic.note(), cymatic.petals(),
                        cymatic.rings(), cymatic.strikeCount());
            check(cymatic.level() > 0.05f, "the cymatic display lights up with the sound");
            check(cymatic.note() >= 40 && cymatic.note() <= 79, "it reads the played note's pitch");
            check(cymatic.petals() >= 3 && cymatic.petals() <= 12,
                  "the figure takes its petal count from the note");
            check(cymatic.strikeCount() >= 1, "it registers the note strike");

            const auto cymaticImage = cymatic.createComponentSnapshot(cymatic.getLocalBounds(), false);
            const int inkPixels = countPixels(cymaticImage, 1, [](juce::Colour c) {
                return c.getAlpha() > 30 && (c.getRed() + c.getGreen() + c.getBlue()) > 90;
            });
            const int cymaticAmber = countPixels(cymaticImage, 1, [](juce::Colour c) {
                return c.getRed() > 200 && c.getGreen() > 100 && c.getGreen() < 190 && c.getBlue() < 120;
            });
            std::printf("  cymatic figure: %d ink pixel(s), %d amber\n", inkPixels, cymaticAmber);
            // The figure as it appears on the panel, and the field image the
            // display renders into: both are kept as artefacts under renders/.
            juce::PNGImageFormat png;
            const juce::File dump("renders/cymatic_field.png");
            dump.deleteFile();
            if (auto stream = std::unique_ptr<juce::FileOutputStream>(dump.createOutputStream()))
                png.writeImageToStream(cymaticImage, *stream);
            const juce::File fieldDump("renders/cymatic_field_raw.png");
            fieldDump.deleteFile();
            if (auto stream = std::unique_ptr<juce::FileOutputStream>(fieldDump.createOutputStream()))
                png.writeImageToStream(cymatic.fieldImageForTest(), *stream);

            check(inkPixels > 400, "the display draws its ripple figure");
            check(cymaticAmber > 40, "the figure is drawn in the panel's amber");

            // Release everything and let it ring away: the plate goes quiet.  The
            // all-notes-off matters — an earlier section of this test leaves a note
            // held, and the instrument is monophonic, so it would otherwise still be
            // sounding here.
            for (int block = 0; block < 80; ++block) {
                juce::AudioBuffer<float> b(2, 512);
                juce::MidiBuffer m;
                if (block == 0) {
                    m.addEvent(juce::MidiMessage::noteOff(1, 60), 0);
                    m.addEvent(juce::MidiMessage::allNotesOff(1), 0);
                }
                processor.processBlock(b, m);
            }
            for (int i = 0; i < 45; ++i) cymatic.tick();
            std::printf("  cymatic after silence: level %.3f, note %d\n",
                        static_cast<double>(cymatic.level()), cymatic.note());
            check(cymatic.level() < 0.06f, "the glow rings away when the instrument is silent");
            check(cymatic.note() == -1, "and it reports no pitch while silent");

            processor.setCurrentProgram(acidBass);
        }

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
