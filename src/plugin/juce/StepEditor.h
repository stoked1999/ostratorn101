// ÖstraTorn101 — the sequencer's step editor.
//
// 16 step slots on the panel, paged through the whole 100-step memory.  The
// interaction follows the hardware idea rather than a piano roll:
//
//   * drag a slot up/down to set its pitch (a fader throw, not a note-off),
//   * click a slot to make it a rest, click a rest to bring it back,
//   * shift-click or right-click to tie the step into the next one,
//   * REC arms step write: notes played on the keyboard are written into the
//     step and advance the write head, the way the instrument records.
//
// The editor never touches the audio thread: it reads and writes the processor's
// message-thread mirror of the sequence, and the processor publishes edits and
// playback telemetry through atomics and the existing pattern hand-over.
#pragma once

#include <juce_gui_basics/juce_gui_basics.h>

#include "OstraTornLookAndFeel.h"
#include "PluginProcessor.h"

// A panel switch with an indicator LED: dark face when off, lit when on.  Used
// for REC (red) and FOLLOW (green); the shared look-and-feel's button drawing
// deliberately ignores toggle state, so toggles are drawn here.
class LedToggleButton : public juce::Button {
public:
    LedToggleButton(const juce::String& name, juce::Colour ledColour);
    void paintButton(juce::Graphics& g, bool shouldDrawButtonAsHighlighted,
                     bool shouldDrawButtonAsDown) override;

private:
    juce::Colour ledColour_;
};

class StepEditor : public juce::Component,
                   public juce::SettableTooltipClient,
                   private juce::Timer {
public:
    static constexpr int kSlots = 16;           // slots shown at once
    static constexpr int kSlotLowNote = 24;     // the display range: C1..
    static constexpr int kSlotHighNote = 84;    // ..C6

    explicit StepEditor(OstraTornAudioProcessor& processor);
    ~StepEditor() override;

    void paint(juce::Graphics& g) override;
    void resized() override;

    void mouseDown(const juce::MouseEvent& e) override;
    void mouseDrag(const juce::MouseEvent& e) override;
    void mouseUp(const juce::MouseEvent& e) override;
    void mouseWheelMove(const juce::MouseEvent& e, const juce::MouseWheelDetails& wheel) override;

    // ---- REC ---------------------------------------------------------------
    void setRecArmed(bool armed);
    bool recArmed() const { return recArmed_; }
    int  writeHead() const { return writeHead_; }

    // ---- Paging ------------------------------------------------------------
    int  page() const { return page_; }
    int  pageCount() const;
    void setPage(int page);

    // Centre of a slot in this component's coordinates; the tests drive the
    // editor's own gestures through it.
    juce::Point<int> slotCentre(int slot) const;

    // Drains the processor's captured keyboard notes into the pattern, follows
    // the playhead and repaints what changed.  Driven by this component's own
    // timer (30 Hz) and called directly by the tests.
    void tick();

private:
    void timerCallback() override { tick(); }

    juce::Rectangle<int> slotsArea() const;
    juce::Rectangle<int> slotBounds(int slot) const;
    juce::Rectangle<int> wellBounds(int slot) const;
    int  slotAt(juce::Point<int> position) const;

    void toggleRest(int slot);
    void toggleTie(int slot);
    void nudgePitch(int slot, int semitones);
    void paintSlot(juce::Graphics& g, int slot, int index, bool inRange, int playStep);
    void updateReadouts();

    OstraTornAudioProcessor& processor_;

    int  page_ = 0;
    int  lastPlayStep_ = -1;
    bool recArmed_ = false;
    int  writeHead_ = 0;

    // Drag state.
    int  dragSlot_ = -1;
    int  dragStartY_ = 0;
    int  dragStartNote_ = 60;
    bool dragChanged_ = false;

    LedToggleButton recButton_{ "REC", ostra::Palette::ledRed };
    LedToggleButton followButton_{ "FOLLOW", ostra::Palette::ledGreen };
    juce::TextButton prevPageButton_{ "<" };
    juce::TextButton nextPageButton_{ ">" };
    juce::Label pageLabel_;
    juce::Label readoutLabel_;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(StepEditor)
};
