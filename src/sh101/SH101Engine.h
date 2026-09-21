// SH101Engine — the complete monophonic voice.
//
// Signal path (brief, "Overall signal path"): VCO -> source mixer -> VCF ->
// VCA -> output amplifier, with the envelope, LFO, keyboard CV, portamento,
// bender, noise source and the CPU-side note logic (arpeggiator/sequencer)
// around it.
//
// The per-sample order below follows the brief's "Processing order per sample"
// list one-to-one; each numbered comment in SH101Engine.cpp names the step it
// implements.  This class is deliberately free of any host framework: the VST3
// wrapper (first-coding-task step 9) only has to move parameter values in and
// audio out.
#pragma once

#include "sh101/AnalogVariation.h"
#include "sh101/Arpeggiator.h"
#include "sh101/Calibration.h"
#include "sh101/Constants.h"
#include "sh101/NoiseGenerator.h"
#include "sh101/OutputStage.h"
#include "sh101/Params.h"
#include "sh101/Portamento.h"
#include "sh101/SH101Envelope.h"
#include "sh101/SH101LFO.h"
#include "sh101/SH101VCA.h"
#include "sh101/SH101VCF.h"
#include "sh101/SH101VCO.h"
#include "sh101/Smoothing.h"
#include "sh101/SourceMixer.h"
#include "sh101/StepSequencer.h"
#include "sh101/SubOscDivider.h"
#include "sh101/VoiceController.h"

namespace sh101 {

class SH101Engine {
public:
    SH101Engine();

    void prepare(double sampleRate, int oversampleFactor = 2);
    void reset();
    void setCalibration(const Calibration& cal);
    const Calibration& calibration() const { return cal_; }

    // Snapshot the parameter set; safe to call between blocks.
    void setParams(const SH101Params& p);
    const SH101Params& params() const { return pending_; }

    // ---- Host / keyboard events -------------------------------------------
    void noteOn(int midiNote, double velocity = 1.0);
    void noteOff(int midiNote);
    void allNotesOff();
    void setSustainPedal(bool down);
    void setPitchBendSemitones(double semitones);   // bender, default range +/-2
    void setVelocityToLevel(bool on) { velocityToLevel_ = on; }  // off by default
    void setArpSyncToHost(bool hostClock);
    void arpHostClockTick();
    void setSeqSyncToHost(bool hostClock);
    void seqHostClockTick();

    // Sequencer programming helpers (host/UI side).
    StepSequencer& sequencer() { return seq_; }
    const StepSequencer& sequencer() const { return seq_; }

    // Analog variation (thermal drift, per-note tolerance, RC tolerance, hiss).
    // On by default: it is part of the instrument's sound.  Tests that measure
    // the calibrated core turn it off explicitly.
    void setAnalogVariationEnabled(bool on);
    bool analogVariationEnabled() const { return cal_.analogVariationEnabled; }
    const AnalogVariation& analogVariation() const { return variation_; }

    // ---- Audio -------------------------------------------------------------
    void renderBlock(float* out, int numSamples);
    float renderSample();

    // ---- Diagnostics (tests and calibration tools) -------------------------
    double sampleRate() const { return sr_; }
    double envelopeValue() const { return envValue_; }
    double lfoValue() const { return lfoValue_; }
    double cutoffHz() const { return vcf_.cutoffHz(); }
    double filterK() const { return vcf_.feedbackK(); }
    double vcaGain() const { return vca_.lastGain(); }
    double oscillatorHz() const { return vco_.frequency(); }
    double subFrequencyHz() const { return vco_.frequency() / sub_.divideRatio(); }
    double mixerSum() const { return mixer_.lastSum(); }
    double lastOutput() const { return lastOut_; }
    double pitchCV() const { return pitchCV_; }
    int    currentNote() const { return sourceNote_; }
    bool   gate() const { return prevGate_; }
    bool   portamentoGliding() const { return porta_.gliding(); }
    double pitchModOctaves() const { return maxPitchModOctaves_; }
    int    oversampleFactor() const { return oversampleFactor_; }

    // Deterministic test mode: every reset re-seeds the noise/LFO generators so
    // renders are bit-reproducible (brief: "deterministic output in test mode").
    void setDeterministicTestMode(bool on, uint32_t seed = 0x1234567u);
    void setNoiseSeed(uint32_t seed);

    // Count of internal safety resets (should stay 0; asserted by tests).
    int  safetyResetCount() const { return safetyResets_; }

private:
    void applyParams();
    double processOne();
    void updateHeldNotesForArp();
    // Starts a note with this key press's tolerance applied: fresh per-note pitch
    // error and RC tolerance, then the gate into the envelope.
    void startNote(bool retrigger);
    // The envelope follows its note source's gate *state*, edge by edge.  Taking
    // the edge from the state and not only from the source's event is what makes
    // a gate that falls without an event still release: the arpeggiator's gate
    // drops inside rebuildPattern() when the last key is released, and a source
    // that is switched off mid-note never reports a fall at all.
    void applySourceGate(bool sourceGate, bool& latched, bool& triggerNow);

    double sr_ = 48000.0;
    int    oversampleFactor_ = 2;
    uint32_t noiseSeed_ = 0x1234567u;
    bool   deterministic_ = true;

    Calibration cal_{};
    SH101Params params_{};
    SH101Params pending_{};

    VoiceController voice_;
    Portamento porta_;
    SH101VCO vco_;
    SubOscDivider sub_;
    NoiseGenerator noise_;
    SourceMixer mixer_;
    SH101VCF vcf_;
    SH101VCA vca_;
    SH101Envelope env_;
    SH101LFO lfo_;
    Arpeggiator arp_;
    StepSequencer seq_;
    OutputStage out_;
    AnalogVariation variation_;

    // Smoothed control values.
    Smoother smCutoffOct_, smRes_, smPw_, smSaw_, smPulse_, smSub_, smNoise_, smVolume_;
    Smoother smTune_, smPitchMod_, smEnvAmt_, smModAmt_, smKeyTrack_, smPwmAmt_;
    Ramp     lfoRateRamp_;

    // Note source state.
    int  targetNote_ = 60;
    int  sourceNote_ = 60;
    bool prevGate_ = false;
    bool seqGate_ = false;
    bool arpGate_ = false;
    bool heldDirty_ = true;
    int  lastSeqStep_ = -1;

    // Host-side modulation.
    double bendSemitones_ = 0.0;
    bool   velocityToLevel_ = false;
    double velocity_ = 1.0;

    // Diagnostics.
    double envValue_ = 0.0;
    double lfoValue_ = 0.0;
    double pitchCV_ = 0.0;
    double lastOut_ = 0.0;
    double maxPitchModOctaves_ = 1.0;   // [APPROX] full-depth LFO pitch modulation
    int    safetyResets_ = 0;
};

} // namespace sh101
