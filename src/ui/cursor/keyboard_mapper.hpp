#pragma once
#include <stdint.h>
#include "ui/performance_state.hpp"

struct KeyMapResult
{
    int16_t pitch; // -1 => function/no note
    bool isScale;
    bool isBlack;
};

class KeyboardMapper
{
public:
    // 2×8 layout; Keyboard: 12 keys + 4 function buttons (Oct−, Oct+, Mode, Hold)
    KeyMapResult map(uint8_t btnId, const PerformanceState &st) const;

private:
    int16_t mapKeyboard(uint8_t btnId, const PerformanceState &st) const;
    int16_t mapScale(uint8_t btnId, const PerformanceState &st) const; // same as Keyboard (flags differ)
    int16_t mapChromatic(uint8_t btnId, const PerformanceState &st) const;
};
