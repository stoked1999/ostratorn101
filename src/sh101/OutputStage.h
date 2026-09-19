// OutputStage — final output amplifier, output level and safety stage.
//
// Hardware basis (brief, processing step 12: "Apply output stage and final
// safety gain").  The analog output amplifier sits after the VCA; the audio
// path is capacitor-coupled, hence the DC blocker (calibration:
// outputDcBlockHz, default 8 Hz).
#pragma once

#include "sh101/Calibration.h"
#include "sh101/Constants.h"

namespace sh101 {

class OutputStage {
public:
    void prepare(double sampleRate, const Calibration& cal = Calibration{});
    void setCalibration(const Calibration& cal) { cal_ = cal; updateFilters(); }
    void reset();

    void setVolume(double v01) { volume_ = clampd(v01, 0.0, 1.0); }

    double process(double x);
    double lastGain() const { return volume_ * cal_.outputGain; }

private:
    void updateFilters();

    double sr_ = 48000.0;
    double volume_ = 0.8;
    double dcCoef_ = 0.0;
    double dcX1_ = 0.0;
    double dcY1_ = 0.0;
    Calibration cal_{};
};

} // namespace sh101
