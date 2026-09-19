#include "sh101/SourceMixer.h"

namespace sh101 {

void SourceMixer::prepare(double sampleRate, const Calibration& cal) {
    (void)sampleRate;
    cal_ = cal;
    reset();
}

void SourceMixer::reset() {
    lastSum_ = 0.0;
    lastOutput_ = 0.0;
}

void SourceMixer::setLevels(double sawLevel, double pulseLevel, double subLevel, double noiseLevel) {
    sawGain_ = clampd(sawLevel, 0.0, 1.0) * cal_.sawMixGain;
    pulseGain_ = clampd(pulseLevel, 0.0, 1.0) * cal_.pulseMixGain;
    subGain_ = clampd(subLevel, 0.0, 1.0) * cal_.subMixGain;
    noiseGain_ = clampd(noiseLevel, 0.0, 1.0) * cal_.noiseMixGain;
}

double SourceMixer::process(double saw, double pulse, double sub, double noise) {
    const double sum = saw * sawGain_ + pulse * pulseGain_ + sub * subGain_ + noise * noiseGain_;
    lastSum_ = sum;
    // Soft clipping models the finite headroom of the IR3109 input stage; the
    // amount of drive here audibly changes filter character (brief, mixer note).
    const double headroom = cal_.mixerHeadroom > 0.0 ? cal_.mixerHeadroom : 1.0;
    lastOutput_ = headroom * std::tanh(sum / headroom);
    return lastOutput_;
}

} // namespace sh101
