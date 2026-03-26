#pragma once
#include <stdint.h>

// Tempo descriptor — BPM + resolution.
struct Tempo {
    float    bpm{120.f};
    uint16_t tpqn{96};    // ticks per quarter note — must match timebase::PPQN
};

// Microseconds per tick at a given tempo.
// Used by Transport to drive the phase accumulator.
inline uint32_t usPerTick(const Tempo& t) {
    return (uint32_t)(60000000.0f / (t.bpm * t.tpqn));
}
