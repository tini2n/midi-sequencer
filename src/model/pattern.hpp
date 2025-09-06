#pragma once
#include <stdint.h>
#include "track.hpp"
#include "../types.hpp"


struct Pattern
{
    Track track;

    uint8_t steps{64}; // default track steps
    uint8_t grid{16};  // 1/16 when PPQN=96 → 6 ticks; keep symbolic
    float tempo{120.f};

    uint32_t ticks() const { return timebase::ticksPerStep(grid) * steps; }
};
