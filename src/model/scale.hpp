#pragma once
#include <stdint.h>

enum class Scale : uint8_t { None = 0, Dorian = 1, Lydian = 2 };

namespace scale
{
    inline const char *name(Scale s)
    {
        switch (s)
        {
        case Scale::None: return "OFF";
        case Scale::Dorian: return "Dorian";
        case Scale::Lydian: return "Lydian";
        }
        return "?";
    }

    inline uint16_t mask(Scale s)
    {
        // 12-bit mask of allowed semitones relative to root
        // bit n set => semitone n is in scale
        switch (s)
        {
        case Scale::Dorian:
            // 0,2,3,5,7,9,10
            return (1u << 0) | (1u << 2) | (1u << 3) | (1u << 5) | (1u << 7) | (1u << 9) | (1u << 10);
        case Scale::Lydian:
            // 0,2,4,6,7,9,11
            return (1u << 0) | (1u << 2) | (1u << 4) | (1u << 6) | (1u << 7) | (1u << 9) | (1u << 11);
        case Scale::None:
        default:
            return 0;
        }
    }

    inline uint8_t degreeSemitone(Scale s, uint8_t deg7)
    {
        static const uint8_t dorian[7] = {0, 2, 3, 5, 7, 9, 10};
        static const uint8_t lydian[7] = {0, 2, 4, 6, 7, 9, 11};
        deg7 %= 7;
        switch (s)
        {
        case Scale::Dorian: return dorian[deg7];
        case Scale::Lydian: return lydian[deg7];
        case Scale::None: default: return 0;
        }
    }

    inline bool contains(Scale s, uint8_t root, uint8_t pitch)
    {
        if (s == Scale::None) return true; // treat as pass-through
        uint8_t rel = (uint8_t)((pitch + 12u - (root % 12)) % 12u);
        return (mask(s) & (1u << rel)) != 0;
    }
}
