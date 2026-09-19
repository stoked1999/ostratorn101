// Calibration constants for the SH-101 model.
//
// The service notes list a set of internal trimmers.  Each field below is the
// software equivalent of one of those trimmers, so the model has the same
// degrees of freedom the real board has, and so a hardware measurement session
// (fidelity milestone 3) can be used to fit them without touching the DSP.
//
// Service adjustment list reproduced (service-notes page ~31):
//   D/A converter tune / width / linearity, VCO width (octave scaling),
//   VCO tune, range width, pulse width, VCF width/tracking, LFO mod offset.
//
// These are NOT user controls (brief: "expose these only as internal
// calibration constants, not normal user controls").
#pragma once

namespace sh101 {

struct Calibration {
    // ---- D/A converter feeding the keyboard CV (service: "D/A tune/width") --
    double dacTuneCents = 0.0;   // static pitch offset of the whole keyboard CV
    double dacWidth     = 1.0;   // V/oct scaling of the keyboard CV

    // ---- VCO ---------------------------------------------------------------
    double vcoWidth       = 1.0;                                   // octave scaling
    double vcoTuneCents   = 0.0;                                   // master tune
    double rangeWidth[4]  = { 1.0, 1.0, 1.0, 1.0 };                // 16', 8', 4', 2'
    double pulseWidthTrim = 1.0;                                   // PW trimmer

    // ---- VCF ---------------------------------------------------------------
    double vcfWidth       = 1.0;   // cutoff CV scaling (service: "VCF width")
    double vcfTracking    = 1.0;   // key-follow trim (service: "VCF tracking")
    double resSelfOscK    = 4.5;   // feedback coefficient at resonance = 1.0
    double filterFcMinHz  = 10.0;  // documented cutoff range 10 Hz .. 20 kHz
    double filterFcMaxHz  = 20000.0;

    // ---- LFO ---------------------------------------------------------------
    double lfoModOffset = 0.0;     // service: "LFO modulation offset"

    // ---- CEM3340 output scaling -------------------------------------------
    // [APPROX] Milestone-2 target: replace with measured CEM3340 transfer data.
    double cemSawLevel   = 0.95;
    double cemPulseLevel = 1.00;
    double cemSquareDuty = 0.5;    // core square used to clock the sub divider

    // ---- Source mixer / IR3109 input drive --------------------------------
    // [APPROX] The brief warns that four unity gains summed digitally do not
    // reproduce the hardware: the level into the IR3109 changes its nonlinear
    // behaviour, so the mixer runs into an explicit soft-clip stage.
    double sawMixGain    = 0.62;
    double pulseMixGain  = 0.62;
    double subMixGain    = 0.55;
    double noiseMixGain  = 0.45;
    double mixerHeadroom = 3.2;    // knee of the filter-input soft clipper

    // ---- Noise generator (selected 2SC945 + conditioning) ------------------
    double noiseAmp       = 0.8;   // [APPROX] measured amplitude target
    double noiseShelfHz   = 40.0;  // [APPROX] conditioning high-pass corner

    // ---- VCA (BA662A, offset-selected) ------------------------------------
    double vcaGainScale    = 1.0;
    double vcaNonlinearity = 0.20; // [APPROX] 0 = ideal OTA, 1 = hard
    double vcaOffLeakage   = 1.0e-4; // ~-80 dB feedthrough with the VCA closed

    // ---- Output stage ------------------------------------------------------
    // [APPROX] Gain staging is a milestone-3 fitting target.  These values were
    // chosen so a standard patch (saw + sub, moderate resonance, volume at 0.8)
    // peaks near -6 dBFS while an everything-at-maximum patch still reaches the
    // limiter instead of the host's clipper (see tools/verify_renders.py).
    double outputGain       = 1.10;
    double outputHeadroom   = 1.60;
    double outputDcBlockHz  = 8.0;  // coupling caps in the audio path

    // ---- Envelope timing interpretation -----------------------------------
    // Analog ADSR: the attack capacitor charges towards a target *above* the
    // trigger threshold; decay/release discharge exponentially.  A parameter
    // time is defined as the time for the segment to cover 99% of its
    // excursion, which is what makes the documented ranges measurable in tests.
    double envAttackTarget = 1.30;
    double envSettleRatio  = 4.605170186; // ln(100)

    // ---- Oscillator / CPU ------------------------------------------------
    // Analog variation is modelled (AnalogVariation.h) rather than faked with
    // noise: these are the tolerance figures of a *healthy, calibrated* unit.
    // The brief explicitly warns against "exaggerated random drift or
    // instability", so each figure is small and bounded, and every one of them
    // is a milestone-3 fitting target against a measured unit.
    bool   analogVariationEnabled = true;
    double driftPitchCents    = 2.5;    // VCO thermal drift, peak deviation
    double driftPitchHz       = 0.22;   // drift rate (first-order corner)
    double noteToleranceCents = 1.5;    // per-note keyboard CV / D-A tolerance
    double driftCutoffOctaves = 0.012;  // ~14 cents of cutoff drift, peak
    double driftCutoffHz      = 0.15;
    double envTimeTolerance   = 0.012;  // +/-1.2% segment times, RC tolerance
    double noiseFloorDbfs     = -88.0;  // output stage idle noise
};

} // namespace sh101
