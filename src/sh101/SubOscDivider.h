// SubOscDivider — flip-flop derived sub oscillator.
//
// Hardware basis (brief, "Sub oscillator"): the sub oscillator is *not* an
// independent oscillator.  It is derived from the VCO by 4013-family dual
// D-type flip-flop divider logic (with an HD14556B BCD-to-4 decoder in the
// surrounding digital logic), and the service parts list calls out an
// MB84013B / 4013-family dual flip-flop for exactly this.
//
// Consequence for the model: every sub oscillator edge must land on a VCO edge
// and the sub must stay phase-locked to the main oscillator for the lifetime of
// the note.  This is implemented by deriving the sub phase from the *master
// phase plus the divider counter*, so the sub phase ramp is continuous and its
// edges coincide exactly with VCO cycle boundaries:
//
//     q = (masterPhase + cycleIndex) / N          N = 2 (one octave down)
//                                                       4 (two octaves down)
//
// Because q ramps linearly at dt/N, standard polyBLEP band-limiting still
// applies exactly (the fractional edge position is carried by the ramp).
//
// Modes (front panel):
//   0  -1 octave square      (N=2, 50% duty)
//   1  -2 octaves square     (N=4, 50% duty)
//   2  -2 octaves narrow     (N=4, 25% duty — decoder combining flip-flop
//                             outputs, one master cycle high out of four)
#pragma once

#include "sh101/Constants.h"

namespace sh101 {

class SubOscDivider {
public:
    enum Mode { Down1Square = 0, Down2Square = 1, Down2NarrowPulse = 2 };

    void prepare(double sampleRate);
    void reset();                        // gates the flip-flops clear, as on gate-on
    void setMode(int mode);
    int  mode() const { return mode_; }
    void setCorePolarityInverted(bool inv) { coreInverted_ = inv; }

    // Called once per sample with the master oscillator state.  masterWrapped
    // must be true on the sample where the VCO cycle boundary was crossed.
    void process(double masterPhase, bool masterWrapped, double masterDt);

    double value() const { return value_; }
    int    divideRatio() const { return N_; }
    double duty() const { return duty_; }
    // Divider output polarity (flip-flop state), exposed for tests.
    int    flipFlopState() const { return cycle_ % N_; }

private:
    double sr_ = 48000.0;
    int    mode_ = Down1Square;
    int    N_ = 2;
    int    cycle_ = 0;
    double duty_ = 0.5;
    double value_ = -1.0;
    bool   coreInverted_ = false;
    double na_ = -1.0;
};

} // namespace sh101
