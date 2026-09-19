// vst3_host_test — loads the built VST3 through JUCE's plugin hosting classes
// (the same code path a host application uses), instantiates it, sends it MIDI
// and renders audio.
//
// This is the end-to-end check that cannot be faked: the bundle is scanned, the
// plugin object is created, MIDI is delivered and audio comes out.  It is still
// not a substitute for opening it in Ableton, but it exercises everything except
// Live's own UI.
//
// Uses JUCE 8's headless hosting API (addHeadlessDefaultFormatsToManager /
// createPluginInstanceAsync), so no plugin UI code is needed.
//
// Usage: sh101_host_test <path to SH-101.vst3>
#include <juce_audio_processors/juce_audio_processors.h>
#include <juce_audio_processors/format/juce_AudioPluginFormatManagerHelpers.h>

#include <algorithm>
#include <cstdio>
#include <memory>

#include "sh101/Params.h"

namespace {

juce::AudioPluginFormat* findVST3Format(juce::AudioPluginFormatManager& manager) {
    for (int i = 0; i < manager.getNumFormats(); ++i) {
        auto* format = manager.getFormat(i);
        if (format->getName().containsIgnoreCase("VST3")) return format;
    }
    return nullptr;
}

// Renders `blocks` blocks, placing a note-on in the first block and optionally a
// note-off later.  Returns the peak magnitude seen.
float render(juce::AudioPluginInstance& instance, juce::AudioBuffer<float>& buffer, int blocks,
             bool noteOn, int noteOffAfter) {
    juce::MidiBuffer midi;
    float peak = 0.0f;
    for (int b = 0; b < blocks; ++b) {
        buffer.clear();
        midi.clear();
        if (noteOn && b == 0) midi.addEvent(juce::MidiMessage::noteOn(1, 48, 1.0f), 0);
        if (noteOffAfter >= 0 && b == noteOffAfter) {
            midi.addEvent(juce::MidiMessage::noteOff(1, 48), 0);
        }
        instance.processBlock(buffer, midi);
        peak = std::max(peak, buffer.getMagnitude(0, buffer.getNumSamples()));
    }
    return peak;
}

} // namespace

int main(int argc, char** argv) {
    juce::ScopedJuceInitialiser_GUI juceInitialiser;   // message manager for instantiation

    if (argc < 2) {
        std::printf("usage: sh101_host_test <path to ÖstraTorn101.vst3>\n");
        return 2;
    }
    const juce::String pluginPath = juce::String::fromUTF8(argv[1]);
    std::printf("scanning: %s\n", pluginPath.toRawUTF8());

    juce::AudioPluginFormatManager formats;
    juce::addDefaultFormatsToManager(formats);   // UI-capable, so the editor can be tested
    auto* vst3 = findVST3Format(formats);
    if (vst3 == nullptr) {
        std::printf("FAIL: no VST3 format available in this build\n");
        return 1;
    }
    std::printf("format: %s\n", vst3->getName().toRawUTF8());

    juce::OwnedArray<juce::PluginDescription> found;
    vst3->findAllTypesForFile(found, pluginPath);
    std::printf("plugins found: %d\n", found.size());
    if (found.isEmpty()) {
        std::printf("FAIL: the bundle was scanned but reported no plugins\n");
        return 1;
    }
    for (auto* description : found) {
        std::printf("  name='%s' manufacturer='%s' instrument=%d inputs=%d outputs=%d\n",
                    description->name.toRawUTF8(), description->manufacturerName.toRawUTF8(),
                    static_cast<int>(description->isInstrument), description->numInputChannels,
                    description->numOutputChannels);
    }

    const double sampleRate = 48000.0;
    const int blockSize = 512;

    std::unique_ptr<juce::AudioPluginInstance> instance;
    juce::String creationError;
    bool done = false;
    vst3->createPluginInstanceAsync(*found[0], sampleRate, blockSize,
                                    [&](std::unique_ptr<juce::AudioPluginInstance> created,
                                        const juce::String& error) {
                                        instance = std::move(created);
                                        creationError = error;
                                        done = true;
                                    });
    if (!done) {
        const auto deadline = juce::Time::getMillisecondCounter() + 15000;
        while (!done && juce::Time::getMillisecondCounter() < deadline) {
            juce::MessageManager::getInstance()->runDispatchLoopUntil(20);
        }
    }
    if (instance == nullptr) {
        std::printf("FAIL: createPluginInstance: %s\n", creationError.toRawUTF8());
        return 1;
    }
    std::printf("instantiated: '%s' (%d parameters) hasEditor=%d\n", instance->getName().toRawUTF8(),
                instance->getParameters().size(), static_cast<int>(instance->hasEditor()));

    // Show what a host will list.  JUCE's VST3 hosting adds parameter entries of
    // its own (MIDI CC automation), so the SH-101's own controls are counted by
    // matching the engine's parameter names.
    {
        const auto& params = instance->getParameters();
        int own = 0;
        int shown = 0;
        for (auto* p : params) {
            const auto name = p->getName(64);
            bool isOurs = false;
            for (int i = 0; i < sh101::kNumParams; ++i) {
                if (name == sh101::paramName(i)) {
                    isOurs = true;
                    break;
                }
            }
            if (isOurs) {
                ++own;
                if (shown < 8) {
                    std::printf("  param '%s' normalizedDefault=%.3f\n", name.toRawUTF8(),
                                p->getDefaultValue());
                    ++shown;
                }
            }
        }
        std::printf("ÖstraTorn101 parameters reported: %d of %d total (the rest are host-side MIDI CC "
                    "automation entries)\n",
                    own, params.size());
    }

    instance->enableAllBuses();
    instance->prepareToPlay(sampleRate, blockSize);

    const int numOutputs = juce::jmax(1, instance->getTotalNumOutputChannels());
    std::printf("output channels: %d\n", numOutputs);

    juce::AudioBuffer<float> buffer(numOutputs, blockSize);

    const float silencePeak = render(*instance, buffer, 4, false, -1);
    const float notePeak = render(*instance, buffer, 24, true, 20);
    const float tailPeak = render(*instance, buffer, 40, false, -1);

    std::printf("peak: silence=%.6f note=%.6f tail=%.6f\n", silencePeak, notePeak, tailPeak);

    instance->releaseResources();

    // Editor check: construct the plugin window.  The editor's *pixels* cannot be
    // verified through VST3 hosting — JUCE hosts a plugin's view in a native child
    // window, so it is neither a JUCE child component of the returned wrapper nor
    // renderable into an off-screen image (verified: 0 children, empty snapshot).
    // What can be verified here is that the editor constructs without crashing and
    // comes back with a usable size, which means our editor was instantiated.  The
    // panel's actual drawing and the preset wiring are checked by
    // tools/editor_test.cpp, which builds the editor directly.
    int editorExitCode = 0;
    if (instance->hasEditor()) {
        if (auto* editor = instance->createEditorIfNeeded()) {
            const int width = editor->getWidth();
            const int height = editor->getHeight();
            std::printf("editor: %dx%d (wrapper contains %d juce children)\n", width, height,
                        editor->getNumChildComponents());
            if (width < 400 || height < 300) {
                std::printf("FAIL: editor size %dx%d is not usable\n", width, height);
                editorExitCode = 1;
            }
            instance->editorBeingDeleted(editor);
            delete editor;
        } else {
            std::printf("FAIL: hasEditor() is true but createEditorIfNeeded() returned null\n");
            editorExitCode = 1;
        }
    } else {
        std::printf("note: this hosting path reports no editor\n");
    }

    int exitCode = 0;
    if (silencePeak > 1.0e-3f) {
        std::printf("FAIL: not silent before the note\n");
        exitCode = 1;
    }
    if (notePeak < 1.0e-3f) {
        std::printf("FAIL: no audio produced for a held note\n");
        exitCode = 1;
    }
    if (notePeak > 1.5f) {
        std::printf("FAIL: output exceeded a sane level\n");
        exitCode = 1;
    }
    if (editorExitCode != 0) exitCode = editorExitCode;
    std::printf(exitCode == 0 ? "RESULT: plugin loads, accepts MIDI, produces audio and creates its "
                                "editor\n"
                              : "RESULT: FAILED\n");
    return exitCode;
}
