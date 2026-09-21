// SH101HostAdapter — the host-facing layer: MIDI input, parameter automation and
// buffer rendering, with no dependency on any plugin framework.
//
// Why this exists: the brief's first coding task ends with the VST3 wrapper, but
// the platform-independent half of that job — translating host events into the
// engine's monophonic note/gate/trigger model and applying normalized
// automation — can (and should) be written and tested without JUCE.  The JUCE
// VST3 processor in src/plugin/juce is a thin shell over this class.
//
// Modern-host policy implemented here (brief, "Modern-host policy"):
//   * Note On/Off drive the mono note, gate, trigger, priority, legato and
//     portamento engine.
//   * Pitch bend maps to the bender's range (+/-2 semitones by default).
//   * CC and DAW automation control existing SH-101 parameters.
//   * Sustain pedal (CC 64) is supported without breaking the mono gate/legato
//     logic.
//   * All Notes Off (CC 123 / 120) and a panic reset are implemented.
//   * Velocity is accepted but does NOT affect the sound unless explicitly
//     enabled (velocityToLevel, off by default).
//   * Free-running arpeggiator/sequencer timing remains the default; host clock
//     sync is opt-in.
#pragma once

#include <cstdint>

#include "sh101/Params.h"
#include "sh101/SH101Engine.h"

namespace sh101 {

class SH101HostAdapter {
public:
    void prepare(double sampleRate, int maxBlockSize, int oversampleFactor = 2);
    void reset();

    // ---- MIDI -------------------------------------------------------------
    // Raw MIDI bytes (one message per call, running status not required).
    void handleMidiMessage(uint8_t status, uint8_t data1, uint8_t data2);
    // Convenience for hosts that hand over a MIDI buffer.
    void handleMidiBuffer(const uint8_t* bytes, int size);

    // ---- Parameters -------------------------------------------------------
    // Parameters are normalized 0..1 exactly as a host automation lane sees
    // them; the taper to engineering units lives in Params.h.
    void setParameter(int paramId, double normalizedValue);
    double getParameter(int paramId) const;
    void setAllParametersNormalized(double value);   // useful for init/test
    // Bulk automation: applies `count` normalized values (one per ParamId) and
    // commits once, which is what a plugin's processBlock wants.
    void setParametersNormalized(const float* values, int count);

    // ---- Audio ------------------------------------------------------------
    // Renders `numSamples` of the voice into a mono buffer (replaces contents).
    void renderBlock(float* mono, int numSamples);
    // Renders into an N-channel buffer; the SH-101 is monophonic, so every
    // channel receives the same voice (no stereo widening — brief, "Do not add
    // unison, stereo widening ...").
    void renderBlock(float* const* channels, int numChannels, int numSamples);

    // ---- Access -----------------------------------------------------------
    SH101Engine& engine() { return engine_; }
    const SH101Engine& engine() const { return engine_; }
    const SH101Params& params() const { return params_; }
    SH101Params& paramsRef() { return params_; }

    // Pushes the current parameter set into the engine (called automatically
    // after parameter changes; exposed for bulk edits through paramsRef()).
    void commitParameters();

    // ---- Host clock / transport (optional) --------------------------------
    void setArpSyncToHost(bool on);
    void setSeqSyncToHost(bool on);
    void hostClockTick();
    void setHostTempoBpm(double bpm);

    // ---- Diagnostics ------------------------------------------------------
    int  pitchBendRangeSemitones() const { return bendRangeSemitones_; }
    void setPitchBendRangeSemitones(int semitones);
    int  lastMidiNote() const { return lastMidiNote_; }
    double pitchBendSemitones() const { return bendSemitones_; }

private:
    void updateVelocityToLevel();

    SH101Engine engine_{};
    SH101Params params_{};
    double sr_ = 48000.0;
    int maxBlockSize_ = 512;
    int bendRangeSemitones_ = 2;      // [APPROX] the bender's default range
    double bendSemitones_ = 0.0;
    int lastMidiNote_ = -1;
    bool velocityToLevel_ = false;
    long long ccCount_[128] = {};
};

} // namespace sh101
