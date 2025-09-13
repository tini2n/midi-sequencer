#pragma once
#include <stdint.h>

enum class Scale
{
    Chromatic,
    Major,
    Minor
};

inline uint16_t maskFor(Scale s)
{
    switch (s)
    {
    case Scale::Major:
        return 0b101011010101; // C major: 0,2,4,5,7,9,11
    case Scale::Minor:
        return 0b101101011010; // C nat minor: 0,2,3,5,7,8,10
    default:
        return 0x0FFF; // chromatic
    }
}

inline bool pitchInMask(uint8_t pitch, uint16_t mask) { return (mask >> (pitch % 12)) & 1; }

inline uint8_t nthInScale(uint8_t root, uint16_t mask, uint8_t n)
{
    uint8_t p = root, count = 0;
    for (;;)
    {
        if ((mask >> (p % 12)) & 1)
        {
            if (count == n)
                return p;
            ++count;
        }
        ++p;
    }
}
