// Standalone probe for the host-adapter note/gate behaviour (development aid).
//
// Reproduces the MIDI-stream scenario from the adapter tests and prints the
// engine's gate/envelope state after each step, which is how the sustain-pedal
// ordering issue was found.
#include <cstdio>
#include <vector>

#include "plugin/SH101HostAdapter.h"

using namespace sh101;

namespace {

double rmsOf(const std::vector<float>& v) {
    double s = 0.0;
    for (float x : v) s += static_cast<double>(x) * x;
    return std::sqrt(s / static_cast<double>(v.size()));
}

void report(SH101HostAdapter& a, const char* stage) {
    std::printf("%-22s gate=%d note=%3d env=%.3f bend=%+.2f st\n", stage, (int)a.engine().gate(),
                a.engine().currentNote(), a.engine().envelopeValue(), a.pitchBendSemitones());
}

} // namespace

int main() {
    SH101HostAdapter a;
    a.prepare(48000.0, 512, 1);
    SH101Params p;
    p.sawLevel = 1.0;
    p.pulseLevel = 0.0;
    p.subLevel = 0.0;
    p.noiseLevel = 0.0;
    p.cutoff = 2000.0;
    p.resonance = 0.0;
    p.attack = 0.005;
    p.decay = 1.0;
    p.sustain = 1.0;
    p.release = 0.05;
    p.volume = 0.8;
    a.paramsRef() = p;
    a.commitParameters();

    std::vector<float> buf(9600);

    report(a, "start");
    a.handleMidiMessage(0xB0, 64, 127);
    a.handleMidiMessage(0x90, 60, 100);
    a.handleMidiMessage(0xB0, 64, 0);
    a.renderBlock(buf.data(), 9600);
    report(a, "pedal up after note");
    std::printf("rms after pedal-up    = %.6f\n", rmsOf(buf));

    // Same sequence, but slotted inside one MIDI buffer (as the test does).
    SH101HostAdapter b;
    b.prepare(48000.0, 512, 1);
    b.paramsRef() = p;
    b.commitParameters();
    const uint8_t stream[] = {
        0xC0, 5, 0x90, 60, 100, 0xB0, 64, 127, 0xE0, 0, 80, 0x80, 60, 0, 0xB0, 64, 0,
    };
    b.handleMidiBuffer(stream, static_cast<int>(sizeof(stream)));
    report(b, "after buffer");
    b.renderBlock(buf.data(), 9600);
    std::printf("rms after buffer      = %.6f\n", rmsOf(buf));
    report(b, "after render");
    return 0;
}
