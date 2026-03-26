#pragma once
#include <stdint.h>

// Musical scale definitions used by the keyboard and generators.
//
// Scales constrain which pitches are valid. None = chromatic (all pitches pass through).
//
// Mask encoding: 12-bit bitmask, one bit per semitone.
//   bit 0 = root, bit 1 = root+1, ..., bit 11 = root+11
//   e.g. Major mask = 0b101010110101 = semitones {0,2,4,5,7,9,11}

enum class Scale : uint8_t {
    None       = 0,  // no constraint — all 12 semitones pass through
    Major      = 1,  // Ionian:   0,2,4,5,7,9,11
    Minor      = 2,  // Aeolian:  0,2,3,5,7,8,10
    Dorian     = 3,  // Dorian:   0,2,3,5,7,9,10
    Phrygian   = 4,  // Phrygian: 0,1,3,5,7,8,10
    Lydian     = 5,  // Lydian:   0,2,4,6,7,9,11
    PentaMajor = 6,  // Pentatonic Major: 0,2,4,7,9     (5 degrees)
    PentaMinor = 7,  // Pentatonic Minor: 0,3,5,7,10    (5 degrees)
};

namespace scale
{
    // Human-readable name for display on OLED.
    inline const char* name(Scale s) {
        switch (s) {
        case Scale::None:       return "OFF";
        case Scale::Major:      return "Major";
        case Scale::Minor:      return "Minor";
        case Scale::Dorian:     return "Dorian";
        case Scale::Phrygian:   return "Phrygian";
        case Scale::Lydian:     return "Lydian";
        case Scale::PentaMajor: return "Penta Maj";
        case Scale::PentaMinor: return "Penta Min";
        }
        return "?";
    }

    // 12-bit bitmask of semitones in the scale.
    // Bit n is set if semitone n (relative to root) is in the scale.
    inline uint16_t mask(Scale s) {
        switch (s) {
        case Scale::Major:      return (1u<<0)|(1u<<2)|(1u<<4)|(1u<<5)|(1u<<7)|(1u<<9)|(1u<<11);
        case Scale::Minor:      return (1u<<0)|(1u<<2)|(1u<<3)|(1u<<5)|(1u<<7)|(1u<<8)|(1u<<10);
        case Scale::Dorian:     return (1u<<0)|(1u<<2)|(1u<<3)|(1u<<5)|(1u<<7)|(1u<<9)|(1u<<10);
        case Scale::Phrygian:   return (1u<<0)|(1u<<1)|(1u<<3)|(1u<<5)|(1u<<7)|(1u<<8)|(1u<<10);
        case Scale::Lydian:     return (1u<<0)|(1u<<2)|(1u<<4)|(1u<<6)|(1u<<7)|(1u<<9)|(1u<<11);
        case Scale::PentaMajor: return (1u<<0)|(1u<<2)|(1u<<4)|(1u<<7)|(1u<<9);
        case Scale::PentaMinor: return (1u<<0)|(1u<<3)|(1u<<5)|(1u<<7)|(1u<<10);
        case Scale::None:
        default:                return 0;
        }
    }

    // Number of degrees in the scale: 7 for heptatonic, 5 for pentatonic.
    // Used by degreeSemitone() and keyboard fold mode.
    inline uint8_t degreeCount(Scale s) {
        switch (s) {
        case Scale::PentaMajor:
        case Scale::PentaMinor: return 5;
        case Scale::None:       return 12; // chromatic: 12 semitones
        default:                return 7;
        }
    }

    // Map a scale degree (0-based) to a semitone offset from the root.
    // Wraps automatically using degreeCount(s).
    //
    // Used by keyboard fold mode to map 16 buttons across 2 octaves of scale degrees.
    inline uint8_t degreeSemitone(Scale s, uint8_t degree) {
        static const uint8_t major[7]      = {0, 2, 4, 5, 7, 9, 11};
        static const uint8_t minor[7]      = {0, 2, 3, 5, 7, 8, 10};
        static const uint8_t dorian[7]     = {0, 2, 3, 5, 7, 9, 10};
        static const uint8_t phrygian[7]   = {0, 1, 3, 5, 7, 8, 10};
        static const uint8_t lydian[7]     = {0, 2, 4, 6, 7, 9, 11};
        static const uint8_t pentaMaj[5]   = {0, 2, 4, 7, 9};
        static const uint8_t pentaMin[5]   = {0, 3, 5, 7, 10};

        degree %= degreeCount(s);
        switch (s) {
        case Scale::Major:      return major[degree];
        case Scale::Minor:      return minor[degree];
        case Scale::Dorian:     return dorian[degree];
        case Scale::Phrygian:   return phrygian[degree];
        case Scale::Lydian:     return lydian[degree];
        case Scale::PentaMajor: return pentaMaj[degree];
        case Scale::PentaMinor: return pentaMin[degree];
        case Scale::None:
        default:                return degree; // chromatic: degree == semitone
        }
    }

    // Returns true if `pitch` is in the scale rooted at `root`.
    // Always returns true when scale is None (no constraint).
    inline bool contains(Scale s, uint8_t root, uint8_t pitch) {
        if (s == Scale::None) return true;
        uint8_t rel = static_cast<uint8_t>((pitch + 12u - (root % 12u)) % 12u);
        return (mask(s) & (1u << rel)) != 0;
    }

} // namespace scale
