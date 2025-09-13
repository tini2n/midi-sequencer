#pragma once
#include <stdint.h>
#include "music/scale.hpp"

enum class PerfMode
{
    Keyboard,
    Scale,
    Chromatic,
    Chord
};

struct PerformanceState
{
    PerfMode mode{PerfMode::Keyboard};
    uint8_t root{0};
    int8_t octave{4};
    uint16_t mask{maskFor(Scale::Major)};
};

struct MapResult
{
    bool isCtrl = false;
    int8_t octaveDelta = 0;
    uint8_t pitch = 60;
    bool isScale = true;
    bool isBlack = false;
};

class KeyboardMapper
{
public:
    MapResult map(uint8_t btn, const PerformanceState &ps) const;
};
