// Measurement helpers shared by the validation tests.
//
// Everything here is deliberately simple and explainable, because the numbers
// these helpers produce are the evidence the test-suite logs.
#pragma once

#include <cmath>
#include <cstdio>
#include <vector>

#include "sh101/Constants.h"

namespace sh101test {

using sh101::kTwoPi;

// ---- Frequency estimation --------------------------------------------------

// Zero-crossing frequency estimate with hysteresis, robust for saw, pulse and
// sine-like signals.  Hysteresis level is relative to the signal's peak.
inline double zeroCrossFrequency(const std::vector<double>& x, double sampleRate,
                                 double hysteresisFraction = 0.15) {
    if (x.size() < 16) return 0.0;
    double peak = 0.0;
    for (double v : x) peak = std::max(peak, std::fabs(v));
    if (peak <= 1.0e-12) return 0.0;
    const double hyst = peak * hysteresisFraction;

    int crossings = 0;
    int firstIndex = -1;
    int lastIndex = -1;
    bool armed = false;
    for (size_t i = 0; i < x.size(); ++i) {
        if (!armed && x[i] < -hyst) armed = true;
        if (armed && x[i] > hyst) {
            if (firstIndex < 0) firstIndex = static_cast<int>(i);
            lastIndex = static_cast<int>(i);
            ++crossings;
            armed = false;
        }
    }
    if (crossings < 2 || lastIndex <= firstIndex) return 0.0;
    const double periods = static_cast<double>(crossings - 1);
    const double seconds = static_cast<double>(lastIndex - firstIndex) / sampleRate;
    return periods / seconds;
}

// Duty cycle: fraction of samples above zero (used for pulse-width checks).
inline double dutyCycle(const std::vector<double>& x) {
    if (x.empty()) return 0.0;
    size_t high = 0;
    for (double v : x) {
        if (v > 0.0) ++high;
    }
    return static_cast<double>(high) / static_cast<double>(x.size());
}

inline double peakAbs(const std::vector<double>& x) {
    double p = 0.0;
    for (double v : x) p = std::max(p, std::fabs(v));
    return p;
}

inline double rms(const std::vector<double>& x) {
    if (x.empty()) return 0.0;
    double s = 0.0;
    for (double v : x) s += v * v;
    return std::sqrt(s / static_cast<double>(x.size()));
}

inline bool allFinite(const std::vector<double>& x) {
    for (double v : x) {
        if (!std::isfinite(v)) return false;
    }
    return true;
}

// ---- Spectral analysis ----------------------------------------------------

// Goertzel magnitude at a single frequency, with a Hann window applied before
// the transform.  Returns the amplitude (not power) at that bin.
inline double goertzelAmplitude(const std::vector<double>& x, double sampleRate, double freqHz) {
    const size_t n = x.size();
    if (n < 8) return 0.0;
    const double w = kTwoPi * freqHz / sampleRate;
    const double cw = std::cos(w);
    const double coeff = 2.0 * cw;
    double s0 = 0.0, s1 = 0.0, s2 = 0.0;
    double winSum = 0.0;
    for (size_t i = 0; i < n; ++i) {
        const double win = 0.5 - 0.5 * std::cos(kTwoPi * static_cast<double>(i) / static_cast<double>(n - 1));
        winSum += win;
        s0 = (x[i] * win) + coeff * s1 - s2;
        s2 = s1;
        s1 = s0;
    }
    const double power = s1 * s1 + s2 * s2 - coeff * s1 * s2;
    const double mag = std::sqrt(std::max(0.0, power));
    return (winSum > 0.0) ? (2.0 * mag / winSum) : 0.0;
}

// ---- Envelope timing helpers ----------------------------------------------

// Time (seconds) for a sampled envelope to first reach `threshold`.
inline double timeToReach(const std::vector<double>& x, double sampleRate, double threshold) {
    for (size_t i = 0; i < x.size(); ++i) {
        if (x[i] >= threshold) return static_cast<double>(i) / sampleRate;
    }
    return -1.0;
}

// Time (seconds) for a monotonically decaying envelope to fall to `threshold`
// after `startIndex`.
inline double timeToFall(const std::vector<double>& x, double sampleRate, double threshold,
                         size_t startIndex = 0) {
    for (size_t i = startIndex; i < x.size(); ++i) {
        if (std::fabs(x[i]) <= threshold) {
            return static_cast<double>(i - startIndex) / sampleRate;
        }
    }
    return -1.0;
}

// ---- Misc -----------------------------------------------------------------

inline double db(double linear) { return 20.0 * std::log10(std::max(1.0e-12, linear)); }

} // namespace sh101test
