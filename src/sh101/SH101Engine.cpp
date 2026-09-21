#include "sh101/SH101Engine.h"

namespace sh101 {

SH101Engine::SH101Engine() = default;

void SH101Engine::prepare(double sampleRate, int oversampleFactor) {
    sr_ = (sampleRate > 0.0) ? sampleRate : 48000.0;
    oversampleFactor_ = (oversampleFactor == 1 || oversampleFactor == 2 || oversampleFactor == 4)
                            ? oversampleFactor : 2;

    vco_.prepare(sr_, cal_);
    sub_.prepare(sr_);
    noise_.prepare(sr_, cal_);
    mixer_.prepare(sr_, cal_);
    vcf_.prepare(sr_, cal_, oversampleFactor_);
    vca_.prepare(sr_, cal_);
    env_.prepare(sr_, cal_);
    lfo_.prepare(sr_);
    arp_.prepare(sr_);
    seq_.prepare(sr_);
    out_.prepare(sr_, cal_);
    voice_.prepare(sr_);
    porta_.prepare(sr_, cal_);
    variation_.prepare(sr_, cal_);

    for (Smoother* s : { &smCutoffOct_, &smRes_, &smPw_, &smSaw_, &smPulse_, &smSub_, &smNoise_,
                         &smVolume_, &smTune_, &smPitchMod_, &smEnvAmt_, &smModAmt_, &smKeyTrack_,
                         &smPwmAmt_ }) {
        s->prepare(sr_);
        s->setTimeConstantMs(8.0);
    }
    lfoRateRamp_.prepare(sr_, 20.0);

    applyParams();
    reset();
}

void SH101Engine::reset() {
    vco_.reset();
    sub_.reset();
    noise_.reset(noiseSeed_);
    mixer_.reset();
    vcf_.reset();
    vca_.reset();
    env_.reset();
    lfo_.reset(noiseSeed_ ^ 0x10f010f0u);
    arp_.reset();
    seq_.reset();
    out_.reset();
    voice_.reset();
    porta_.reset(midiNoteToCV(targetNote_));
    // The variation stream is seeded from the same seed as the noise/LFO
    // generators, so a deterministic test mode reproduces the drift as well.
    variation_.reset(noiseSeed_ ^ 0x2f1c8d3bu);

    prevGate_ = false;
    seqGate_ = false;
    arpGate_ = false;
    envValue_ = 0.0;
    lfoValue_ = 0.0;
    lastOut_ = 0.0;
    safetyResets_ = 0;

    // Snap smoothed controls to their targets so a reset is immediate.
    smCutoffOct_.snap(std::log2(pending_.cutoff / cal_.filterFcMinHz));
    smRes_.snap(pending_.resonance);
    smPw_.snap(pending_.pulseWidth);
    smSaw_.snap(pending_.sawLevel);
    smPulse_.snap(pending_.pulseLevel);
    smSub_.snap(pending_.subLevel);
    smNoise_.snap(pending_.noiseLevel);
    smVolume_.snap(pending_.volume);
    smTune_.snap(pending_.vcoTune);
    smPitchMod_.snap(pending_.vcoModDep);
    smEnvAmt_.snap(pending_.filterEnvAmt);
    smModAmt_.snap(pending_.filterModAmt);
    smKeyTrack_.snap(pending_.keyTrack);
    smPwmAmt_.snap(pending_.pwmAmount);
    lfoRateRamp_.set(pending_.lfoRate);
}

void SH101Engine::setCalibration(const Calibration& cal) {
    cal_ = cal;
    vco_.setCalibration(cal);
    noise_.setCalibration(cal);
    mixer_.setCalibration(cal);
    vcf_.setCalibration(cal);
    vca_.setCalibration(cal);
    env_.setCalibration(cal);
    out_.setCalibration(cal);
    porta_.prepare(sr_, cal);
    variation_.setCalibration(cal);
}

void SH101Engine::setAnalogVariationEnabled(bool on) {
    cal_.analogVariationEnabled = on;
    variation_.setCalibration(cal_);
}

void SH101Engine::setParams(const SH101Params& p) {
    pending_ = p;
    applyParams();
}

// Block-rate parameter application (state changes that cannot be smoothed
// sample-by-sample), plus smoother targets for everything else.
void SH101Engine::applyParams() {
    // Block-rate: envelope times / mode, LFO wave, VCA mode, portamento, arp/seq.
    env_.setAttack(pending_.attack);
    env_.setDecay(pending_.decay);
    env_.setSustain(pending_.sustain);
    env_.setRelease(pending_.release);
    env_.setTriggerMode(pending_.envTrigger);

    lfo_.setWave(pending_.lfoWave);
    lfoRateRamp_.moveTo(pending_.lfoRate);

    vca_.setMode(pending_.vcaMode);
    out_.setVolume(pending_.volume);

    porta_.setTime(pending_.portamentoTime);
    porta_.setMode(pending_.portamentoMode);

    vco_.setRange(pending_.vcoRange);
    vco_.setFineTuneCents(pending_.vcoTune);
    sub_.setMode(pending_.subMode);

    arp_.setEnabled(pending_.arpOn);
    arp_.setMode(pending_.arpMode);
    arp_.setRate(pending_.arpRate);
    arp_.setOctaves(pending_.arpOctaves);
    seq_.setEnabled(pending_.seqOn);
    // The sequencer runs on a musical tempo: its own BPM, or the host's when it
    // is set to follow the host (and the host has reported one).
    const double seqTempoBpm = (pending_.seqSync != 0 && hostTempoBpm_ > 0.0)
                                   ? hostTempoBpm_
                                   : pending_.seqBpm;
    seq_.setRate(seqStepRateHz(seqTempoBpm, pending_.seqDivision));
    // v1.0 step-editor controls: the sequence's length, gate and transpose are
    // host parameters, so they arrive with every parameter commit.
    seq_.setLength(pending_.seqLength);
    seq_.setGateLength(pending_.seqGate);
    seq_.setTranspose(pending_.seqTranspose);

    // Sample-rate-smoothed: cutoff, resonance, levels, depths, tune.
    smCutoffOct_.setTarget(std::log2(clampd(pending_.cutoff, cal_.filterFcMinHz, cal_.filterFcMaxHz)
                                     / cal_.filterFcMinHz));
    smRes_.setTarget(pending_.resonance);
    smPw_.setTarget(pending_.pulseWidth);
    smSaw_.setTarget(pending_.sawLevel);
    smPulse_.setTarget(pending_.pulseLevel);
    smSub_.setTarget(pending_.subLevel);
    smNoise_.setTarget(pending_.noiseLevel);
    smVolume_.setTarget(pending_.volume);
    smTune_.setTarget(pending_.vcoTune);
    smPitchMod_.setTarget(pending_.vcoModDep);
    smEnvAmt_.setTarget(pending_.filterEnvAmt);
    smModAmt_.setTarget(pending_.filterModAmt);
    smKeyTrack_.setTarget(pending_.keyTrack);
    smPwmAmt_.setTarget(pending_.pwmAmount);
}

void SH101Engine::noteOn(int midiNote, double velocity) {
    velocity_ = velocity;
    voice_.noteOn(midiNote, velocity);
    heldDirty_ = true;
}

void SH101Engine::noteOff(int midiNote) {
    voice_.noteOff(midiNote);
    heldDirty_ = true;
}

void SH101Engine::allNotesOff() {
    voice_.allNotesOff();
    heldDirty_ = true;
}

void SH101Engine::setSustainPedal(bool down) {
    voice_.setSustainPedal(down);
    heldDirty_ = true;
}

void SH101Engine::setPitchBendSemitones(double semitones) {
    // Host pitch bend; the default range matches the instrument's bender
    // (modern-host policy: bend maps naturally to the original behaviour).
    bendSemitones_ = clampd(semitones, -24.0, 24.0);
}

void SH101Engine::setHostTempoBpm(double bpm) {
    hostTempoBpm_ = (bpm > 0.0) ? bpm : 0.0;
}

void SH101Engine::setArpSyncToHost(bool hostClock) { arp_.setExternalClock(hostClock); }
void SH101Engine::arpHostClockTick() { arp_.advanceExternalClock(); }
void SH101Engine::setSeqSyncToHost(bool hostClock) { seq_.setExternalClock(hostClock); }
void SH101Engine::seqHostClockTick() { seq_.advanceExternalClock(); }

void SH101Engine::setDeterministicTestMode(bool on, uint32_t seed) {
    deterministic_ = on;
    noiseSeed_ = seed;
    noise_.reset(seed);
    lfo_.reset(seed ^ 0x10f010f0u);
}

void SH101Engine::setNoiseSeed(uint32_t seed) {
    noiseSeed_ = seed;
}

void SH101Engine::updateHeldNotesForArp() {
    arp_.setHeldNotes(voice_.heldNotes(), voice_.heldCount());
    heldDirty_ = false;
}

void SH101Engine::applySourceGate(bool sourceGate, bool& latched, bool& triggerNow) {
    if (sourceGate == latched) return;
    latched = sourceGate;
    if (sourceGate) {
        triggerNow = true;
        startNote(pending_.envTrigger == SH101Envelope::GateAndTrig);
    } else {
        env_.noteOff();
    }
}

// Every note start — keyboard, arpeggiator step or sequencer step — draws this
// note's analog tolerance: a fixed small pitch error from the keyboard CV path
// and a small RC variation on the envelope segment times.  Both are deterministic
// for a given seed, so test renders stay reproducible.
void SH101Engine::startNote(bool retrigger) {
    variation_.noteOn();
    env_.setTimeScale(variation_.envelopeTimeScale());
    env_.noteOn(retrigger);
}

double SH101Engine::processOne() {
    // ==== 0. Analog variation: drift and noise floor, once per sample ========
    // Per sample (not per block) so the result is independent of the host's
    // buffer size, which the block-size-independence test asserts.
    variation_.process();

    // ==== 1. Note / gate / trigger state ====================================
    // Note source selection: sequencer, then arpeggiator, then the keyboard.
    bool gateNow = false;
    bool triggerNow = false;
    bool legatoNow = false;
    bool noteChangedNow = false;

    // A source that is switched off mid-note never reports its gate falling — its
    // process() is no longer called at all — so release it here, or the note is
    // left gated with nothing driving it.
    if (!pending_.seqOn && seqGate_) {
        seqGate_ = false;
        env_.noteOff();
    }
    if (!pending_.arpOn && arpGate_) {
        arpGate_ = false;
        env_.noteOff();
    }

    if (pending_.seqOn) {
        const StepSequencer::Event ev = seq_.process();
        if (ev.noteChange) {
            sourceNote_ = seq_.currentNote();
            lastSeqStep_ = seq_.currentIndex();
            noteChangedNow = true;
        }
        // The gate comes from the sequencer's own state, so a rest, a tie or a
        // pattern that runs out still releases the note.
        applySourceGate(seq_.gateHigh(), seqGate_, triggerNow);
        gateNow = seqGate_;
    } else if (pending_.arpOn) {
        if (heldDirty_) updateHeldNotesForArp();
        const Arpeggiator::Event ev = arp_.process();
        if (ev.noteChange) {
            sourceNote_ = arp_.currentNote();
            noteChangedNow = true;
        }
        // ...and likewise here: releasing the last key empties the arpeggiator's
        // pattern, and its gate falls with no event to say so.
        applySourceGate(arp_.gateHigh(), arpGate_, triggerNow);
        gateNow = arpGate_;
    } else {
        // Keyboard: consume the ordered gate transitions.  A host hands over the
        // MIDI for a block *before* that block is rendered, so a note that starts
        // and ends inside one sample (routine at small buffer sizes) must still
        // start and then release the envelope; a state-only check would miss the
        // edge and leave the envelope running with the gate already low.
        bool rise = false;
        while (voice_.takeGateEvent(rise)) {
            if (rise) {
                triggerNow = true;
                startNote(pending_.envTrigger == SH101Envelope::GateAndTrig);
            } else {
                env_.noteOff();
            }
        }
        gateNow = voice_.gateHigh();
        if (voice_.triggerEvent()) {
            triggerNow = true;
            if (voice_.currentNote() >= 0) {
                sourceNote_ = voice_.currentNote();
                noteChangedNow = true;
            }
        } else if (voice_.noteChanged() && voice_.currentNote() >= 0) {
            sourceNote_ = voice_.currentNote();
            noteChangedNow = true;
        }
        legatoNow = voice_.legato();
        velocity_ = voice_.velocity();
        voice_.consumeEvents();
    }

    // GATE+TRIG mode: a note taken while the gate is already high retriggers the
    // envelope (every note-on is a trigger), while GATE mode lets it continue.
    if (legatoNow && gateNow && pending_.envTrigger == SH101Envelope::GateAndTrig) {
        env_.noteOn(true);
    }

    // ==== 2. Target pitch CV (1 V/oct) ======================================
    if (noteChangedNow) {
        // AUTO portamento glides only in a legato context.  That is either a new
        // key taken while the gate was already high (VoiceController's legato
        // flag) or a return to a note that was still held (no new trigger).
        const bool legatoContext = gateNow && (legatoNow || !triggerNow);
        porta_.setAutoEligible(legatoContext && !pending_.arpOn && !pending_.seqOn);
    }
    const double keyCV = midiNoteToCV(static_cast<double>(sourceNote_));
    const double dacTrimmedCV = keyCV * cal_.dacWidth + cal_.dacTuneCents / 1200.0;

    // ==== 3. Portamento =====================================================
    const double cvPortamento = porta_.process(dacTrimmedCV);

    // ==== 4. LFO / random / noise control sources ===========================
    lfo_.setRate(lfoRateRamp_.process());
    lfo_.process();
    lfoValue_ = lfo_.value();

    // ==== 5. Envelope =======================================================
    env_.setModGate(lfo_.squareHigh());       // LFO trigger mode
    envValue_ = env_.process();

    // ==== 6. Pitch CV -> frequency, then the oscillator =====================
    const double pitchMod = smPitchMod_.process() * maxPitchModOctaves_ * lfoValue_;
    const double bendCV = bendSemitones_ / 12.0;
    // AnalogVariation: thermal drift plus this note's keyboard-CV tolerance, in
    // cents.  The CV is in octaves, so cents/1200.
    const double analogPitchCV = variation_.pitchOffsetCents() / 1200.0;
    pitchCV_ = cvPortamento + pitchMod + bendCV + analogPitchCV;
    vco_.setFineTuneCents(smTune_.process());
    vco_.setPitchCV(pitchCV_);

    // Pulse width and its modulation source (ENV / MANUAL / LFO).
    const double basePw = smPw_.process();
    double width = basePw;
    // [APPROX] PWM polarity/depth are not transcribed from the schematic; the
    // modulation spans +/-40% of the pulse period around the manual setting.
    const double pwmDepth = smPwmAmt_.process() * 0.8;
    if (pending_.pwmSource == 0) {
        width = basePw + pwmDepth * (envValue_ - 0.5);
    } else if (pending_.pwmSource == 2) {
        width = basePw + pwmDepth * (lfo_.unipolar() - 0.5);
    }
    vco_.setPulseWidth(clampd(width * cal_.pulseWidthTrim, 0.03, 0.97));
    vco_.process();

    // ==== 7. Phase-locked sub oscillator ====================================
    const double masterDt = clampd(vco_.frequency() / sr_, 0.0, 0.5);
    sub_.process(vco_.phase(), vco_.wrapped(), masterDt);

    // ==== 8. Source mixer (hardware-inspired gains + filter-input drive) ====
    const double noiseSample = noise_.process();
    mixer_.setLevels(smSaw_.process(), smPulse_.process(), smSub_.process(), smNoise_.process());
    const double mixed = mixer_.process(vco_.saw() * cal_.cemSawLevel,
                                        vco_.pulse() * cal_.cemPulseLevel,
                                        sub_.value(),
                                        noiseSample);

    // ==== 9. Filter cutoff CV: manual + envelope + LFO + key follow + bend ==
    const double manualOctaves = smCutoffOct_.process();
    const double keyFollow = midiNoteToCV(static_cast<double>(sourceNote_))
                             * smKeyTrack_.process() * cal_.vcfTracking;
    const double cutoffOctaves = (manualOctaves
                                  + smEnvAmt_.process() * envValue_
                                  + smModAmt_.process() * lfoValue_
                                  + keyFollow) * cal_.vcfWidth
                                 + variation_.cutoffOffsetOctaves();   // VCF control-path drift
    vcf_.setCutoffHz(cal_.filterFcMinHz * std::exp2(cutoffOctaves));
    vcf_.setResonance(smRes_.process());

    // ==== 10. IR3109-inspired four-pole VCF (with oversampled feedback) =====
    const double filtered = vcf_.process(mixed);

    // ==== 11. BA662-inspired VCA (ENV or GATE control) ======================
    vca_.setGate(gateNow);
    double vcaOut = vca_.process(filtered, envValue_);
    if (velocityToLevel_) vcaOut *= velocity_;

    // ==== 12. Output stage and final safety gain ============================
    out_.setVolume(smVolume_.process());
    // The output amplifier's own noise floor (AnalogVariation): added after the
    // VCA, which is where the hiss of the real instrument comes from.
    double y = out_.process(vcaOut) + variation_.noiseFloor();

    // Safety net: a non-finite sample means a state blew up; clear the analog
    // states instead of passing NaN on to the host.  Tests assert this counter
    // stays at zero.
    if (!std::isfinite(y)) {
        ++safetyResets_;
        vcf_.reset();
        out_.reset();
        y = 0.0;
    }

    prevGate_ = gateNow;
    lastOut_ = y;
    return y;
}

void SH101Engine::renderBlock(float* out, int numSamples) {
    for (int i = 0; i < numSamples; ++i) {
        out[i] = static_cast<float>(processOne());
    }
}

float SH101Engine::renderSample() {
    return static_cast<float>(processOne());
}

} // namespace sh101
