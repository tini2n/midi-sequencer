#pragma once
#include "step_transport.hpp"
#include "step_pattern.hpp"
#include "core/midi_io.hpp"

// Minimal playback: NoteOn at step edge, NoteOff after fixed gate.
namespace step {

struct Playback {
    int lastOn{-1};
    uint16_t gateLeft{0};

    void processStep(const Transport&, const Pattern& pat, MidiIO& midi, uint16_t tps_now) {
        const Track& tr = pat.track();
        if (gateLeft) {
            if (--gateLeft == 0 && lastOn >= 0) {
                midi.send({tr.channel, static_cast<uint8_t>(tr.pitch), 0, false, 0});
                lastOn = -1;
            }
        }

        // Step edge detection via transport
        static uint16_t step = 0;
        step = (step + 1) & 127;
        if (tr.steps.test(step)) {
            midi.send({tr.channel, static_cast<uint8_t>(tr.pitch), 100, true, 0});
            gateLeft = tps_now / 2; // 50% gate
            lastOn = step;
        }
    }
};

} // namespace step
