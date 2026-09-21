#include "CymaticDisplay.h"

#include <cmath>

using namespace ostra;

namespace {

// Bessel function of the first kind, J_n(x).  The power series is more than
// enough here (n <= 14, x <= 18) and keeps the display free of dependencies.
double besselJ(int n, double x) {
    if (x < 0.0) x = -x;                       // J_n is even in x for even n...
    n = std::abs(n);
    const double half = x * 0.5;

    double term = 1.0;                         // (x/2)^n / n!
    for (int i = 1; i <= n; ++i) term *= half / static_cast<double>(i);
    double sum = term;
    for (int k = 1; k < 64; ++k) {
        term *= -(half * half) / (static_cast<double>(k) * static_cast<double>(n + k));
        sum += term;
        if (std::abs(term) < 1.0e-13) break;
    }
    return sum;
}

// The m-th positive zero of J_n(x), by the standard asymptotic expansion.  This
// is the plate's radial wavenumber: the figure then has exactly `m` concentric
// nodes from the centre to the rim, and the rim itself is a node — which is what
// makes the disc read as a struck plate rather than as a drawn target.
double besselZero(int n, int m) {
    const double order = n;
    const double index = juce::jmax(1, m);
    const double a = (index + order * 0.5 - 0.25) * juce::MathConstants<double>::pi;
    const double mu = 4.0 * order * order;
    return a - (mu - 1.0) / (8.0 * a)
           - (4.0 * (mu - 1.0) * (7.0 * mu - 31.0)) / (3.0 * std::pow(8.0 * a, 3.0));
}

// Deterministic pseudo-random grain in 0.7..1.3, so the filaments read like
// caustics on water rather than a clean plot.
float grainFor(int index) {
    juce::uint32 hash = static_cast<juce::uint32>(index) * 2654435761u;
    hash ^= hash >> 13;
    hash *= 2246822519u;
    hash ^= hash >> 16;
    return 0.70f + 0.60f * static_cast<float>(hash & 0xffffu) / 65535.0f;
}

// The pitch of a monophonic signal, by the normalised difference function (a
// plain autocorrelation is too easily fooled by harmonics).  Returns a MIDI
// note, or -1 when there is no clear period.
int detectNote(const float* x, int count, double sampleRate) {
    constexpr int kStride = 2;
    const int m = count / kStride;
    if (m < 512) return -1;

    const double decimatedRate = sampleRate / kStride;
    const int minLag = static_cast<int>(decimatedRate / 900.0);
    const int maxLag = juce::jmin(m / 2, static_cast<int>(decimatedRate / 55.0));
    if (minLag < 8 || maxLag <= minLag) return -1;

    double best = 1.0;
    int bestLag = -1;
    for (int lag = minLag; lag <= maxLag; ++lag) {
        double difference = 0.0;
        double energy = 0.0;
        const int samples = m - lag;
        for (int i = 0; i < samples; ++i) {
            const double a = x[i * kStride];
            const double b = x[(i + lag) * kStride];
            const double d = a - b;
            difference += d * d;
            energy += a * a + b * b;
        }
        if (energy < 1.0e-12) continue;
        const double value = difference / energy;
        if (value < best) {
            best = value;
            bestLag = lag;
        }
    }
    // A weak "period" is a noise floor, not a note.
    if (bestLag < 0 || best > 0.30) return -1;

    const double frequency = decimatedRate / bestLag;
    if (frequency < 20.0 || frequency > 5000.0) return -1;
    return static_cast<int>(std::lround(69.0 + 12.0 * std::log2(frequency / 440.0)));
}

} // namespace

CymaticDisplay::CymaticDisplay(OstraTornAudioProcessor& processor) : processor_(processor) {
    setTooltip("The cymatic display: the sound made visible. The figure is the standing wave of "
               "the note being played - its petals follow the pitch, its rings the brightness - "
               "and every note strike sends a ripple through the pattern.");

    rebuildField();
    startTimerHz(30);
}

CymaticDisplay::~CymaticDisplay() {
    stopTimer();
}

void CymaticDisplay::resized() {
    const float diameter = juce::jmin(getWidth(), getHeight()) - 10.0f;
    const int wanted =
        juce::jlimit(64, kFieldMaxSize, static_cast<int>(std::lround(diameter)));
    if (wanted != fieldSize_) {
        fieldSize_ = wanted;
        rebuildField();
        repaint();
    }
}

// The field is rendered at (close to) the size it is drawn at, so its filaments
// survive the trip to the window.
void CymaticDisplay::rebuildField() {
    fieldImage_ = juce::Image(juce::Image::ARGB, fieldSize_, fieldSize_, true);

    // Per-cell geometry: a cell's distance from the centre indexes the radial
    // tables, its angle the angular ones.  Computed once per size.
    const float half = fieldSize_ * 0.5f;
    for (int y = 0; y < fieldSize_; ++y) {
        for (int x = 0; x < fieldSize_; ++x) {
            const int cell = y * fieldSize_ + x;
            const float nx = (x + 0.5f - half) / half;
            const float ny = (y + 0.5f - half) / half;
            const float radius = std::sqrt(nx * nx + ny * ny);
            if (radius > 1.0f) {
                radiusIndex_[static_cast<size_t>(cell)] = 255;   // off the plate
                continue;
            }
            radiusIndex_[static_cast<size_t>(cell)] = static_cast<juce::uint8>(
                juce::jlimit(0, kRadialSamples - 1,
                             static_cast<int>(radius * (kRadialSamples - 1) + 0.5f)));

            float angle = std::atan2(ny, nx);
            if (angle < 0.0f) angle += juce::MathConstants<float>::twoPi;
            thetaIndex_[static_cast<size_t>(cell)] = static_cast<juce::uint16>(
                juce::jlimit(0, kThetaSamples - 1,
                             static_cast<int>(angle / juce::MathConstants<float>::twoPi
                                              * kThetaSamples)));
            grain_[static_cast<size_t>(cell)] = grainFor(cell);
        }
    }

    updateModeTables();
    renderField();
}

// ---- Analysis ---------------------------------------------------------------
void CymaticDisplay::analyse() {
    const double sampleRate = processor_.getSampleRate() > 0.0 ? processor_.getSampleRate() : 48000.0;
    const int got = processor_.readScopeSamples(window_.data(), kWindowSamples);

    double sumSquares = 0.0;
    int crossings = 0;
    float previous = 0.0f;
    for (int i = 0; i < got; ++i) {
        const float value = window_[i];
        sumSquares += static_cast<double>(value) * value;
        if (i > 0 && (value >= 0.0f) != (previous >= 0.0f)) ++crossings;
        previous = value;
    }
    const float rms = got > 0 ? static_cast<float>(std::sqrt(sumSquares / static_cast<double>(got)))
                              : 0.0f;
    levelTarget_ = juce::jlimit(0.0f, 1.0f, rms * 9.0f);   // the voice sits well below full scale

    // A strike is the signal rising out of the floor.  The model's own output
    // hiss sits near -88 dBFS, so this threshold is comfortably above it.
    const bool gate = rms > 0.004f;
    if (gate && ! gateOn_) {
        ++strikes_;
        strikeAge_ = 0.0f;
        strikeAmplitude_ = 1.0f;
    }
    gateOn_ = gate;

    // Brightness: the zero-crossing rate, mapped across the instrument's range.
    const double seconds = (got > 0) ? static_cast<double>(got) / sampleRate : 0.0;
    const double crossingRate = seconds > 0.0 ? static_cast<double>(crossings) / seconds : 0.0;
    const float brightness =
        juce::jlimit(0.0f, 1.0f, static_cast<float>((crossingRate - 150.0) / 2600.0));

    // Pitch: how a cymatic plate gains nodes with the drive frequency, the petal
    // count follows the note being played.
    const int detected = gate ? detectNote(window_.data(), got, sampleRate) : -1;
    if (detected >= 0) {
        note_ = detected;
        petalsTarget_ = juce::jlimit(3, 10, 3 + (detected - 33) / 6);
    } else if (! gate) {
        note_ = -1;
        petalsTarget_ = 6;        // at rest, the plate settles to a calm figure
    }

    ringsTarget_ = gate ? juce::jlimit(2, 5, 2 + static_cast<int>(std::lround(brightness * 3.0f)))
                        : 3;
}

// ---- The figure -------------------------------------------------------------
void CymaticDisplay::updateModeTables() {
    // A standing wave on a disc: J_n(k r) · cos(nθ + φ).  The radial part gives
    // the concentric rings, the angular part the petals, and a second mode plus a
    // J_0 term keeps the filigree organic instead of a perfect star.
    const double k1 = besselZero(petals_, rings_);
    const double k2 = besselZero(petals_ + 2, juce::jmax(1, rings_ - 1));
    const double k3 = besselZero(0, rings_);

    double maxAbs1 = 1.0e-9, maxAbs2 = 1.0e-9, maxAbs3 = 1.0e-9;
    for (int i = 0; i < kRadialSamples; ++i) {
        const double r = static_cast<double>(i) / (kRadialSamples - 1);
        radial1_[static_cast<size_t>(i)] = static_cast<float>(besselJ(petals_, k1 * r));
        radial2_[static_cast<size_t>(i)] = static_cast<float>(besselJ(petals_ + 2, k2 * r));
        radial3_[static_cast<size_t>(i)] = static_cast<float>(besselJ(0, k3 * r));
        maxAbs1 = std::max(maxAbs1, std::abs(static_cast<double>(radial1_[static_cast<size_t>(i)])));
        maxAbs2 = std::max(maxAbs2, std::abs(static_cast<double>(radial2_[static_cast<size_t>(i)])));
        maxAbs3 = std::max(maxAbs3, std::abs(static_cast<double>(radial3_[static_cast<size_t>(i)])));
    }
    // Normalise so the figure has the same weight whatever the mode is.
    const double scale = 1.0 / (maxAbs1 + 0.35 * maxAbs2 + 0.18 * maxAbs3);
    for (int i = 0; i < kRadialSamples; ++i) {
        radial1_[static_cast<size_t>(i)] *= static_cast<float>(scale);
        radial2_[static_cast<size_t>(i)] *= static_cast<float>(0.35 * scale);
        radial3_[static_cast<size_t>(i)] *= static_cast<float>(0.18 * scale);
    }

    // The angular tables carry the swirl phase, so the figure turns slowly.
    const int n1 = petals_;
    const int n2 = petals_ + 2;
    for (int j = 0; j < kThetaSamples; ++j) {
        const double angle = juce::MathConstants<double>::twoPi * j / kThetaSamples;
        theta1_[static_cast<size_t>(j)] = static_cast<float>(std::cos(n1 * angle + phase_));
        theta2_[static_cast<size_t>(j)] =
            static_cast<float>(std::cos(-n2 * angle + 0.7 + 0.6 * phase_));
    }
}

void CymaticDisplay::renderField() {
    // The glow: quiet playing is a faint figure, loud playing a bright one, and a
    // strike lights it up as the ripple crosses it.
    const float amplitude =
        juce::jlimit(0.10f, 1.6f, 0.10f + level_ * (0.85f + 0.6f * strikeAmplitude_));
    const float rippleRadius = 0.12f + strikeAge_ * 0.55f;
    const float rippleWeight = strikeAmplitude_;

    // Pass one: the standing wave itself, and the range of values it actually
    // reaches.  A sum of modes never goes to zero everywhere, so the filaments
    // are picked out by contrast within this figure rather than by a fixed
    // threshold (which would show either everything or nothing).
    float minMagnitude = 1.0e9f;
    float maxMagnitude = 1.0e-6f;
    for (int cell = 0; cell < fieldSize_ * fieldSize_; ++cell) {
        const juce::uint8 radiusIndex = radiusIndex_[static_cast<size_t>(cell)];
        if (radiusIndex == 255) {
            field_[static_cast<size_t>(cell)] = -1.0f;      // off the plate
            continue;
        }
        const size_t ri = radiusIndex;
        const size_t ti = thetaIndex_[static_cast<size_t>(cell)];
        const float value = radial1_[ri] * theta1_[ti] + radial2_[ri] * theta2_[ti]
                            + radial3_[ri];
        const float magnitude = std::abs(value);
        field_[static_cast<size_t>(cell)] = magnitude;
        minMagnitude = std::min(minMagnitude, magnitude);
        maxMagnitude = std::max(maxMagnitude, magnitude);
    }
    const float spread = std::max(1.0e-4f, maxMagnitude - minMagnitude);

    // Pass two: the filaments — the still lines of the wave — as lit amber.
    fieldImage_.clear(fieldImage_.getBounds(), juce::Colours::transparentBlack);
    juce::Image::BitmapData pixels(fieldImage_, juce::Image::BitmapData::writeOnly);

    const juce::Colour deep(0xff54230a);
    const juce::Colour mid(0xffe8862a);
    const juce::Colour hot(0xffffe0b4);

    for (int y = 0; y < fieldSize_; ++y) {
        for (int x = 0; x < fieldSize_; ++x) {
            const int cell = y * fieldSize_ + x;
            const float magnitude = field_[static_cast<size_t>(cell)];
            if (magnitude < 0.0f) continue;

            const float closeness =
                juce::jlimit(0.0f, 1.0f, 1.0f - (magnitude - minMagnitude) / spread);
            // The glow follows the level, but a filament keeps a hot core: that is
            // what makes the plate read as lit rather than as a dim plot.
            float ink = std::pow(closeness, 1.6f) * grain_[static_cast<size_t>(cell)]
                        * (0.45f + 0.85f * amplitude);

            // The strike ripple brightens the pattern as it passes.
            if (rippleWeight > 0.01f) {
                const juce::uint8 radiusIndex = radiusIndex_[static_cast<size_t>(cell)];
                const float radius = static_cast<float>(radiusIndex) / (kRadialSamples - 1);
                const float distance = radius - rippleRadius;
                ink *= 1.0f + 1.3f * rippleWeight * std::exp(-distance * distance * 16.0f);
            }
            if (ink <= 0.012f) continue;

            ink = juce::jmin(1.0f, ink);
            // Amber ramp: deep ember, the panel's amber, then a pale gold core.
            const juce::Colour colour = (ink < 0.62f)
                                            ? deep.interpolatedWith(mid, ink / 0.62f)
                                            : mid.interpolatedWith(hot, (ink - 0.62f) / 0.38f);
            // Fully opaque cores, translucent edges: the plate's bright filaments
            // have to survive the composite over the dark window.
            pixels.setPixelColour(x, y, colour.withAlpha(juce::jmin(1.0f, ink * 2.4f)));
        }
    }
}

// ---- Animation --------------------------------------------------------------
void CymaticDisplay::tick() {
    analyse();

    // Ballistics: the glow is struck up quickly and rings away slowly.
    const float attack = 0.55f;
    const float release = 0.10f;
    level_ += (levelTarget_ - level_) * (levelTarget_ > level_ ? attack : release);
    strikeAmplitude_ *= 0.90f;
    if (strikeAmplitude_ < 0.01f) strikeAmplitude_ = 0.0f;
    strikeAge_ += 1.0f / 30.0f;

    // The figure settles toward its targets one node at a time — a plate being
    // driven, not a hard cut.
    if (petals_ < petalsTarget_) ++petals_;
    else if (petals_ > petalsTarget_) --petals_;
    if (rings_ < ringsTarget_) ++rings_;
    else if (rings_ > ringsTarget_) --rings_;

    // A slow swirl; louder playing drives the plate harder.
    phase_ += 0.015f + 0.08f * level_;
    if (phase_ > juce::MathConstants<float>::twoPi) phase_ -= juce::MathConstants<float>::twoPi;

    updateModeTables();
    renderField();
    repaint();
}

// ---- Drawing ----------------------------------------------------------------
void CymaticDisplay::paint(juce::Graphics& g) {
    const auto bounds = getLocalBounds().toFloat();
    drawTintedDisplay(g, bounds);

    // The plate: a circle of light set into the window.
    const float diameter = juce::jmin(bounds.getWidth(), bounds.getHeight()) - 10.0f;
    const auto disc = juce::Rectangle<float>(diameter, diameter).withCentre(bounds.getCentre());

    if (fieldImage_.isValid()) {
        // The plate is a circle, so the figure is clipped to it.
        juce::Path clip;
        clip.addEllipse(disc);
        g.saveState();
        g.reduceClipRegion(clip);
        g.drawImage(fieldImage_, disc, juce::RectanglePlacement::stretchToFit, false);
        g.restoreState();
    }

    // The rim, and the standing ripple of the last strike.
    g.setColour(Palette::accent.withAlpha(0.16f + 0.22f * juce::jlimit(0.0f, 1.0f, level_)));
    g.drawEllipse(disc.reduced(0.5f), 1.2f);

    if (strikeAmplitude_ > 0.03f) {
        const float radius = disc.getWidth() * 0.5f * juce::jmin(1.0f, 0.12f + strikeAge_ * 0.55f);
        g.setColour(Palette::accent.withAlpha(juce::jlimit(0.0f, 0.5f, strikeAmplitude_ * 0.5f)));
        g.drawEllipse(disc.getCentreX() - radius, disc.getCentreY() - radius, radius * 2.0f,
                      radius * 2.0f, 1.4f);
    }
}
