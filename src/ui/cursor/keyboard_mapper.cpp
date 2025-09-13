#include "keyboard_mapper.hpp"

// Layout policy:
// bottom 0..7: C D E F G A B + CTRL(Mode)
// top    8..15: C# D#   F# G# A# + CTRL(Oct-) CTRL(Oct+) CTRL(Hold)
static const int8_t semitoneForBtn[16] = {
    0, 2, 4, 5, 7, 9, 11, -128, // -128 means control
    1, 3, -128, 6, 8, 10, -128, -128
};

MapResult KeyboardMapper::map(uint8_t btn, const PerformanceState &ps) const
{
    MapResult r{};
    // controls
    if (btn == 7)
    {
        r.isCtrl = true;
        r.octaveDelta = 0;
        return r;
    } // Mode (stub)
    if (btn == 10)
    {
        r.isCtrl = true;
        r.octaveDelta = -1;
        return r;
    } // Oct-
    if (btn == 14)
    {
        r.isCtrl = true;
        r.octaveDelta = +1;
        return r;
    } // Oct+
    if (btn == 15)
    {
        r.isCtrl = true;
        r.octaveDelta = 0;
        return r;
    } // Hold (future)

    // note map
    uint8_t base = (uint8_t)((ps.octave * 12 + ps.root) & 0x7F);
    if (ps.mode == PerfMode::Chromatic)
    {
        // map across scale degrees (skip non-scale)
        uint8_t deg = btn;
        r.pitch = nthInScale(base, ps.mask, deg);
    }
    else
    {
        int8_t semi = semitoneForBtn[btn];
        if (semi < -10)
        {
            r.isCtrl = true;
            return r;
        }
        r.pitch = (uint8_t)(base + semi);
    }

    r.isScale = ((ps.mask >> (r.pitch % 12)) & 1);
    uint8_t k = r.pitch % 12;
    r.isBlack = (k == 1 || k == 3 || k == 6 || k == 8 || k == 10);
    
    return r;
}
