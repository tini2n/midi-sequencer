#pragma once
#include <stdint.h>
#include <vector>
#include "ui/performance_state.hpp"

struct ChordMap
{
    uint8_t count;
    uint8_t pitches[4];
};

struct ChordMapper
{
    ChordMap map(uint8_t btnId, const PerformanceState &st) const
    {
        (void)btnId; // stub: single note
        uint8_t p = (uint8_t)(st.octave * 12 + st.root);
        return {1, {p, 0, 0, 0}};
    }
};
