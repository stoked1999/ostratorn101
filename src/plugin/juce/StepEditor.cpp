#include "StepEditor.h"

#include <cmath>

using namespace ostra;

namespace {

// Matches the section panels of the main panel, so the step editor reads as part
// of the same instrument.
constexpr int kPadding = 9;
constexpr int kHeaderHeight = 22;
constexpr int kTransportWidth = 208;
constexpr int kGap = 12;
constexpr int kStepNumberHeight = 13;

// Vertical position of a note inside a slot's well.  The display range covers
// four octaves, which is what a slot can show usefully; notes outside it sit at
// the end stops (they still play at their real pitch).
float noteToWellY(int note, juce::Rectangle<int> well) {
    const float low = static_cast<float>(StepEditor::kSlotLowNote);
    const float high = static_cast<float>(StepEditor::kSlotHighNote);
    const float t = juce::jlimit(0.0f, 1.0f, (static_cast<float>(note) - low) / (high - low));
    const float top = static_cast<float>(well.getY()) + 8.0f;
    const float bottom = static_cast<float>(well.getBottom()) - 8.0f;
    return bottom - t * (bottom - top);
}

int wellYToNote(float y, juce::Rectangle<int> well) {
    const float low = static_cast<float>(StepEditor::kSlotLowNote);
    const float high = static_cast<float>(StepEditor::kSlotHighNote);
    const float top = static_cast<float>(well.getY()) + 8.0f;
    const float bottom = static_cast<float>(well.getBottom()) - 8.0f;
    const float t = (bottom - top) > 0.0f ? juce::jlimit(0.0f, 1.0f, (bottom - y) / (bottom - top))
                                          : 0.0f;
    return juce::jlimit(0, 127, static_cast<int>(std::lround(low + t * (high - low))));
}

} // namespace

// ---- LedToggleButton --------------------------------------------------------
LedToggleButton::LedToggleButton(const juce::String& name, juce::Colour ledColour)
    : juce::Button(name), ledColour_(ledColour) {
    setClickingTogglesState(true);
}

void LedToggleButton::paintButton(juce::Graphics& g, bool highlighted, bool down) {
    const auto r = getLocalBounds().toFloat().reduced(0.5f);
    const bool on = getToggleState();

    juce::Colour face = on ? juce::Colour(0xff2e2e33) : juce::Colour(0xff3f3f44);
    if (down) face = face.darker(0.25f);
    else if (highlighted) face = face.brighter(0.14f);

    g.setGradientFill(juce::ColourGradient(face.brighter(0.20f), r.getX(), r.getY(),
                                           face.darker(0.25f), r.getX(), r.getBottom(), false));
    g.fillRoundedRectangle(r, 3.5f);
    g.setColour(juce::Colours::black.withAlpha(0.55f));
    g.drawRoundedRectangle(r.reduced(0.5f), 3.5f, 1.0f);

    drawLed(g, { r.getX() + 14.0f, r.getCentreY() }, 4.5f, ledColour_, on);

    g.setColour(on ? juce::Colours::white : Palette::displayText.withAlpha(0.85f));
    g.setFont(engravedFont(12.0f));
    g.drawText(getButtonText(), getLocalBounds().withTrimmedLeft(26), juce::Justification::centred,
               false);
}

// ---- StepEditor -------------------------------------------------------------
StepEditor::StepEditor(OstraTornAudioProcessor& processor) : processor_(processor) {
    setTooltip("Step editor: drag a slot to set its pitch, click for a rest, "
               "shift-click to tie. REC writes played notes in.");

    recButton_.setTooltip("Arm record: notes played on the keyboard are written into the "
                          "step and advance the write head");
    recButton_.onClick = [this] { setRecArmed(recButton_.getToggleState()); };
    addAndMakeVisible(recButton_);

    followButton_.setTooltip("Follow the playhead: the page turns itself while the "
                             "sequence runs (paging by hand turns this off)");
    followButton_.setToggleState(true, juce::dontSendNotification);
    addAndMakeVisible(followButton_);

    prevPageButton_.setTooltip("Previous 16 steps");
    prevPageButton_.onClick = [this] { setPage(page_ - 1); };
    addAndMakeVisible(prevPageButton_);

    nextPageButton_.setTooltip("Next 16 steps");
    nextPageButton_.onClick = [this] { setPage(page_ + 1); };
    addAndMakeVisible(nextPageButton_);

    pageLabel_.setFont(labelFont(11.0f));
    pageLabel_.setColour(juce::Label::textColourId, Palette::engrave);
    pageLabel_.setJustificationType(juce::Justification::centred);
    addAndMakeVisible(pageLabel_);

    readoutLabel_.setFont(engravedFont(10.0f));
    readoutLabel_.setColour(juce::Label::textColourId, Palette::engraveSoft);
    readoutLabel_.setJustificationType(juce::Justification::centred);
    addAndMakeVisible(readoutLabel_);

    updateReadouts();
    startTimerHz(30);
}

StepEditor::~StepEditor() {
    stopTimer();
}

juce::Rectangle<int> StepEditor::slotsArea() const {
    auto inner = getLocalBounds().reduced(kPadding);
    inner.removeFromTop(kHeaderHeight);
    inner.removeFromLeft(kTransportWidth + kGap);
    return inner;
}

juce::Rectangle<int> StepEditor::slotBounds(int slot) const {
    const auto area = slotsArea();
    if (area.getWidth() <= 0 || area.getHeight() <= 0) return {};
    const int width = area.getWidth() / kSlots;
    return juce::Rectangle<int>(area.getX() + slot * width, area.getY(), width, area.getHeight());
}

juce::Rectangle<int> StepEditor::wellBounds(int slot) const {
    auto cell = slotBounds(slot).reduced(3, 3);
    cell.removeFromBottom(kStepNumberHeight);   // the step number's own row
    return cell;
}

int StepEditor::slotAt(juce::Point<int> position) const {
    const auto area = slotsArea();
    if (! area.contains(position)) return -1;
    const int width = area.getWidth() / kSlots;
    if (width <= 0) return -1;
    return juce::jlimit(0, kSlots - 1, (position.x - area.getX()) / width);
}

juce::Point<int> StepEditor::slotCentre(int slot) const {
    const auto bounds = slotBounds(juce::jlimit(0, kSlots - 1, slot));
    return { bounds.getCentreX(), bounds.getCentreY() };
}

int StepEditor::pageCount() const {
    return juce::jmax(1, (processor_.patternLength() + kSlots - 1) / kSlots);
}

void StepEditor::setPage(int page) {
    const int clamped = juce::jlimit(0, pageCount() - 1, page);
    if (clamped != page_) {
        page_ = clamped;
        repaint();
    }
    // Paging by hand is the user taking over: stop following the playhead.
    followButton_.setToggleState(false, juce::dontSendNotification);
}

void StepEditor::setRecArmed(bool armed) {
    recArmed_ = armed;
    if (recButton_.getToggleState() != armed) {
        recButton_.setToggleState(armed, juce::dontSendNotification);
    }
    updateReadouts();
    repaint();
}

// ---- Editing ----------------------------------------------------------------
void StepEditor::toggleRest(int slot) {
    const int index = page_ * kSlots + slot;
    if (index >= processor_.patternLength()) return;
    const auto& step = processor_.patternStep(index);
    const bool makeNote = ! step.gate;
    // A rest cannot be tied, so bringing a note back keeps its tie while making
    // a rest drops it.
    processor_.setPatternStep(index, step.note, makeNote, makeNote ? step.tie : false);
    repaint();
}

void StepEditor::toggleTie(int slot) {
    const int index = page_ * kSlots + slot;
    if (index >= processor_.patternLength()) return;
    const auto& step = processor_.patternStep(index);
    // A tie is a note being held, so a tied rest becomes a note.
    processor_.setPatternStep(index, step.note, true, ! step.tie);
    repaint();
}

void StepEditor::nudgePitch(int slot, int semitones) {
    const int index = page_ * kSlots + slot;
    if (index >= processor_.patternLength()) return;
    const auto& step = processor_.patternStep(index);
    processor_.setPatternStep(index, juce::jlimit(0, 127, step.note + semitones), true, step.tie);
    repaint();
}

void StepEditor::mouseDown(const juce::MouseEvent& e) {
    const int slot = slotAt(e.getPosition());
    if (slot < 0) return;
    const int index = page_ * kSlots + slot;
    if (index >= processor_.patternLength()) return;   // past the end of the sequence

    if (e.mods.isPopupMenu() || e.mods.isShiftDown()) {
        toggleTie(slot);
        return;
    }

    dragSlot_ = slot;
    dragStartY_ = e.y;
    dragStartNote_ = processor_.patternStep(index).note;
    dragChanged_ = false;
}

void StepEditor::mouseDrag(const juce::MouseEvent& e) {
    if (dragSlot_ < 0) return;
    const auto well = wellBounds(dragSlot_);
    if (well.isEmpty()) return;

    // One semitone per pixel-per-semitone of travel; dragging up raises the note.
    const float semitonesPerPixel =
        static_cast<float>(kSlotHighNote - kSlotLowNote) / static_cast<float>(juce::jmax(1, well.getHeight() - 16));
    const int delta = static_cast<int>(std::lround(static_cast<float>(dragStartY_ - e.y) / semitonesPerPixel));
    const int index = page_ * kSlots + dragSlot_;
    const auto& step = processor_.patternStep(index);
    const int note = juce::jlimit(0, 127, dragStartNote_ + delta);

    // Dragging is programming a note: a rest under the drag becomes gated.
    if (note != step.note || ! step.gate) {
        processor_.setPatternStep(index, note, true, step.tie);
        dragChanged_ = true;
        repaint();
    }
}

void StepEditor::mouseUp(const juce::MouseEvent&) {
    if (dragSlot_ < 0) return;
    // A click that never moved is a rest toggle.
    if (! dragChanged_) toggleRest(dragSlot_);
    dragSlot_ = -1;
    repaint();
}

void StepEditor::mouseWheelMove(const juce::MouseEvent& e, const juce::MouseWheelDetails& wheel) {
    const int slot = slotAt(e.getPosition());
    if (slot < 0) return;
    const int direction = (wheel.deltaY > 0.0f) ? 1 : (wheel.deltaY < 0.0f ? -1 : 0);
    if (direction != 0) nudgePitch(slot, direction);
}

// ---- Playback ---------------------------------------------------------------
void StepEditor::tick() {
    // 1. REC: notes the host delivered are written into the pattern.  While the
    //    sequencer runs the write head is the played step (real-time recording);
    //    stopped, it advances note by note.
    OstraTornAudioProcessor::CapturedNote note;
    bool wrote = false;
    while (processor_.takeCapturedNote(note)) {
        if (! recArmed_ || ! note.on || note.note < 0) continue;
        const int length = juce::jmax(1, processor_.patternLength());
        const int target = (note.step >= 0 && note.step < length) ? note.step : (writeHead_ % length);
        processor_.setPatternStep(target, note.note, true, false);
        writeHead_ = (target + 1) % length;
        wrote = true;
    }

    // 2. The playhead: follow it across pages while the sequence runs.
    const bool running = processor_.sequenceRunning();
    const int playStep = running ? processor_.sequencePlayStep() : -1;
    if (followButton_.getToggleState() && playStep >= 0) {
        const int page = playStep / kSlots;
        if (page != page_ && page < pageCount()) {
            page_ = page;
            repaint();
        }
    }
    if (page_ >= pageCount()) {          // a shortened sequence can strand the page
        page_ = pageCount() - 1;
        repaint();
    }

    updateReadouts();
    if (wrote || playStep != lastPlayStep_) {
        lastPlayStep_ = playStep;
        repaint();
    }
}

void StepEditor::updateReadouts() {
    pageLabel_.setText("PAGE " + juce::String(page_ + 1) + "/" + juce::String(pageCount()),
                       juce::dontSendNotification);

    const bool running = processor_.sequenceRunning();
    const juce::String play = running ? juce::String(processor_.sequencePlayStep()).paddedLeft('0', 2)
                                      : juce::String("--");
    const juce::String rec = recArmed_ ? juce::String(writeHead_).paddedLeft('0', 2)
                                       : juce::String("--");
    readoutLabel_.setText("PLAY " + play + "  REC " + rec, juce::dontSendNotification);
}

// ---- Drawing ----------------------------------------------------------------
void StepEditor::paintSlot(juce::Graphics& g, int slot, int index, bool inRange, int playStep) {
    const auto cell = slotBounds(slot);
    const auto well = wellBounds(slot);
    if (cell.isEmpty() || well.isEmpty()) return;

    const auto& step = processor_.patternStep(index);
    const bool playing = inRange && playStep == index;

    // Recessed well, the same window-in-metal language as the preset display.
    g.setGradientFill(juce::ColourGradient(Palette::displayTop, static_cast<float>(well.getX()),
                                           static_cast<float>(well.getY()), Palette::displayBottom,
                                           static_cast<float>(well.getX()),
                                           static_cast<float>(well.getBottom()), false));
    g.fillRoundedRectangle(well.toFloat(), 2.5f);
    g.setColour(juce::Colours::black.withAlpha(0.5f));
    g.drawRoundedRectangle(well.toFloat().reduced(0.5f), 2.5f, 1.0f);

    if (! inRange) {
        // Past the end of the sequence: dimmed and inert.
        g.setColour(juce::Colours::black.withAlpha(0.32f));
        g.fillRoundedRectangle(well.toFloat(), 2.5f);
    }

    if (playing) {
        g.setColour(Palette::accent.withAlpha(0.20f));
        g.fillRoundedRectangle(well.toFloat(), 2.5f);
    }

    if (inRange && step.gate) {
        // Pitch: a bar from the foot of the well to the note, capped the way the
        // panel's faders are.
        const float noteY = noteToWellY(step.note, well);
        const juce::Rectangle<float> bar(static_cast<float>(well.getCentreX()) - 5.0f, noteY, 10.0f,
                                         static_cast<float>(well.getBottom()) - noteY - 2.0f);
        g.setColour(Palette::accent.withAlpha(0.40f));
        g.fillRect(bar);

        const juce::Rectangle<float> cap(static_cast<float>(well.getCentreX()) - 11.0f, noteY - 7.0f,
                                         22.0f, 14.0f);
        g.setGradientFill(juce::ColourGradient(Palette::capTop, cap.getX(), cap.getY(),
                                               Palette::capBottom, cap.getX(), cap.getBottom(), false));
        g.fillRoundedRectangle(cap, 2.5f);
        g.setColour(Palette::accent);
        g.fillRect(cap.getX() + 1.0f, cap.getCentreY() - 2.5f, cap.getWidth() - 2.0f, 5.0f);
        g.setColour(juce::Colours::black.withAlpha(0.55f));
        g.drawRoundedRectangle(cap.reduced(0.5f), 2.5f, 1.0f);

        // Tie: the cap grows a bar that runs into the next step — the hold.
        if (step.tie) {
            g.setColour(Palette::accent);
            g.fillRect(cap.getRight(), cap.getCentreY() - 2.0f,
                       static_cast<float>(cell.getRight()) - cap.getRight() + 2.0f, 4.0f);
        }
    }

    // The note's name, and the step number under the well.
    g.setFont(controlNameFont(11.0f));
    g.setColour(inRange ? (step.gate ? Palette::displayText : Palette::displayText.withAlpha(0.45f))
                        : Palette::displayText.withAlpha(0.25f));
    auto nameRow = well.reduced(1, 2);
    nameRow.removeFromBottom(15);
    const bool showNote = inRange && step.gate;
    g.drawText(showNote ? juce::MidiMessage::getMidiNoteName(step.note, true, true, 4)
                        : juce::String("-"),
               nameRow, juce::Justification::centred, false);

    auto numberRow = cell;
    numberRow.removeFromBottom(kStepNumberHeight);
    g.setFont(labelFont(9.0f));
    g.setColour(Palette::engraveSoft.withAlpha(inRange ? 0.95f : 0.55f));
    g.drawText(juce::String(index), numberRow, juce::Justification::centred, false);

    // The playhead: a lit foot on the well and an amber surround.
    if (playing) {
        g.setColour(Palette::accent);
        g.fillRect(juce::Rectangle<float>(static_cast<float>(well.getX()),
                                          static_cast<float>(well.getBottom() - 4),
                                          static_cast<float>(well.getWidth()), 4.0f));
        g.setColour(Palette::ledAmber);
        g.drawRoundedRectangle(well.toFloat().reduced(0.5f), 2.5f, 2.0f);
    }

    // The write head, where REC's next note lands.
    if (recArmed_ && inRange && index == writeHead_ % juce::jmax(1, processor_.patternLength())) {
        g.setColour(Palette::ledRed);
        g.drawRoundedRectangle(well.toFloat().reduced(1.0f), 2.5f, 2.0f);
    }
}

void StepEditor::paint(juce::Graphics& g) {
    drawSectionPanel(g, getLocalBounds().toFloat(), true);

    auto header = getLocalBounds().reduced(kPadding).removeFromTop(kHeaderHeight);
    g.setColour(Palette::engrave);
    g.setFont(engravedFont(11.0f));
    g.drawText("SEQUENCER - STEP EDITOR", header, juce::Justification::centredLeft, false);
    g.setColour(Palette::engraveSoft.withAlpha(0.5f));
    g.fillRect(header.getX(), header.getBottom() - 2, juce::jmin(header.getWidth(), 200), 1);

    const int length = processor_.patternLength();
    const int first = page_ * kSlots;
    const bool running = processor_.sequenceRunning();
    const int playStep = running ? processor_.sequencePlayStep() : -1;

    for (int slot = 0; slot < kSlots; ++slot) {
        const int index = first + slot;
        paintSlot(g, slot, index, index < length, playStep);
    }
}

void StepEditor::resized() {
    auto inner = getLocalBounds().reduced(kPadding);
    inner.removeFromTop(kHeaderHeight);

    auto transport = inner.removeFromLeft(kTransportWidth);
    recButton_.setBounds(transport.removeFromTop(34).reduced(2, 2));
    transport.removeFromTop(6);

    auto pageRow = transport.removeFromTop(26);
    prevPageButton_.setBounds(pageRow.removeFromLeft(30).reduced(1));
    nextPageButton_.setBounds(pageRow.removeFromRight(30).reduced(1));
    pageLabel_.setBounds(pageRow.reduced(6, 0));
    transport.removeFromTop(6);

    followButton_.setBounds(transport.removeFromTop(26).reduced(2, 0));
    transport.removeFromTop(8);
    readoutLabel_.setBounds(transport.removeFromTop(20).reduced(2, 0));
}
