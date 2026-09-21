#include "plugin/SH101HostAdapter.h"

namespace sh101 {

void SH101HostAdapter::prepare(double sampleRate, int maxBlockSize, int oversampleFactor) {
    sr_ = (sampleRate > 0.0) ? sampleRate : 48000.0;
    maxBlockSize_ = maxBlockSize > 0 ? maxBlockSize : 512;
    engine_.prepare(sr_, oversampleFactor);
    engine_.setDeterministicTestMode(false, 0x1234567u);   // free-running noise in a host
    engine_.setVelocityToLevel(velocityToLevel_);
    commitParameters();
}

void SH101HostAdapter::reset() {
    engine_.allNotesOff();
    bendSemitones_ = 0.0;
    engine_.setPitchBendSemitones(0.0);
}

void SH101HostAdapter::commitParameters() {
    engine_.setParams(params_);
}

void SH101HostAdapter::setParameter(int paramId, double normalizedValue) {
    if (paramId < 0 || paramId >= kNumParams) return;
    applyNormalized(params_, paramId, normalizedValue);
    if (paramId == pVolume) params_.volume = clampd(normalizedValue, 0.0, 1.0);
    commitParameters();
}

double SH101HostAdapter::getParameter(int paramId) const {
    if (paramId < 0 || paramId >= kNumParams) return 0.0;
    return getNormalized(params_, paramId);
}

void SH101HostAdapter::setAllParametersNormalized(double value) {
    for (int id = 0; id < kNumParams; ++id) applyNormalized(params_, id, value);
}

void SH101HostAdapter::setParametersNormalized(const float* values, int count) {
    if (!values) return;
    const int n = clampi(count, 0, kNumParams);
    for (int id = 0; id < n; ++id) applyNormalized(params_, id, static_cast<double>(values[id]));
    commitParameters();
}

void SH101HostAdapter::updateVelocityToLevel() {
    engine_.setVelocityToLevel(velocityToLevel_);
}

void SH101HostAdapter::setPitchBendRangeSemitones(int semitones) {
    bendRangeSemitones_ = clampi(semitones, 0, 24);
}

void SH101HostAdapter::handleMidiMessage(uint8_t status, uint8_t data1, uint8_t data2) {
    const uint8_t type = status & 0xF0;
    switch (type) {
        case 0x90: {   // Note On (velocity 0 == Note Off, per MIDI practice)
            if (data2 == 0) {
                engine_.noteOff(data1);
            } else {
                engine_.noteOn(data1, data2 / 127.0);
                lastMidiNote_ = data1;
            }
            break;
        }
        case 0x80:     // Note Off
            engine_.noteOff(data1);
            break;
        case 0xB0: {   // Control Change
            ++ccCount_[data1 & 0x7F];
            switch (data1) {
                case 64:   // sustain pedal
                    engine_.setSustainPedal(data2 >= 64);
                    break;
                case 120:  // All Sound Off
                case 123:  // All Notes Off
                    engine_.allNotesOff();
                    break;
                case 1:    // mod wheel -> LFO modulation depth (the bender's MOD
                           // switch on the original routes the LFO)
                    params_.vcoModDep = data2 / 127.0;
                    params_.filterModAmt = params_.vcoModDep * 2.0;
                    commitParameters();
                    break;
                case 5:    // portamento time
                    params_.portamentoTime = clampd((data2 / 127.0) * 5.0, 0.0, 5.0);
                    commitParameters();
                    break;
                default:
                    break;
            }
            break;
        }
        case 0xE0: {   // Pitch bend
            const int value = (static_cast<int>(data2) << 7) | static_cast<int>(data1);
            const double normalized = (value - 8192) / 8192.0;   // -1..+1
            bendSemitones_ = normalized * bendRangeSemitones_;
            engine_.setPitchBendSemitones(bendSemitones_);
            break;
        }
        default:
            break;
    }
}

void SH101HostAdapter::handleMidiBuffer(const uint8_t* bytes, int size) {
    if (!bytes || size < 2) return;
    // Accepts a stream of complete messages (3 bytes for the messages we use;
    // programme change / channel pressure are 2 bytes and are ignored).
    int i = 0;
    while (i < size) {
        const uint8_t status = bytes[i];
        if (status < 0x80) {   // running status is not tracked: skip invalid byte
            ++i;
            continue;
        }
        const uint8_t type = status & 0xF0;
        if (type == 0xC0 || type == 0xD0) {
            i += 2;   // 2-byte messages: ignored, no data2
            continue;
        }
        if (i + 2 >= size) break;
        handleMidiMessage(status, bytes[i + 1], bytes[i + 2]);
        i += 3;
    }
}

void SH101HostAdapter::setArpSyncToHost(bool on) { engine_.setArpSyncToHost(on); }
void SH101HostAdapter::setSeqSyncToHost(bool on) { engine_.setSeqSyncToHost(on); }
void SH101HostAdapter::hostClockTick() {
    engine_.arpHostClockTick();
    engine_.seqHostClockTick();
}

void SH101HostAdapter::setHostTempoBpm(double bpm) { engine_.setHostTempoBpm(bpm); }

void SH101HostAdapter::renderBlock(float* mono, int numSamples) {
    engine_.renderBlock(mono, numSamples);
}

void SH101HostAdapter::renderBlock(float* const* channels, int numChannels, int numSamples) {
    if (numChannels <= 0 || !channels || !channels[0]) return;
    engine_.renderBlock(channels[0], numSamples);
    for (int ch = 1; ch < numChannels; ++ch) {
        if (!channels[ch]) continue;   // a host may hand over a null spare channel
        for (int i = 0; i < numSamples; ++i) channels[ch][i] = channels[0][i];
    }
}

} // namespace sh101
