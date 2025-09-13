#include "keyboard_mapper.hpp"
#include "music/scale.hpp"

static inline bool isBlackPC(uint8_t pc) { return (pc == 1 || pc == 3 || pc == 6 || pc == 8 || pc == 10); }

// 16 serial order: 1..8 q..i -> btn 0..15
// Keyboard/Scale: 12 active (7 whites on top, 5 blacks on bottom digits), others inactive
//  whites: q w e r t y u  -> pc: 0,2,4,5,7,9,11   (i inactive)
//  blacks: 2 3 5 6 7      -> pc: 1,3,6,8,10      (1,4,8 inactive)
static const int8_t kPC_Keyboard[16] = {
    /*1..8*/ -1, 1, 3, -1, 6, 8, 10, -1,
    /*q..i*/ 0, 2, 4, 5, 7, 9, 11, -1};

int16_t KeyboardMapper::mapKeyboard(uint8_t id, const PerformanceState &st) const
{
    if (id > 15)
        return -1;
    int8_t pc = kPC_Keyboard[id];
    if (pc < 0)
        return -1;
    // return int16_t(st.octave * 12 + (st.root + pc) % 12);
    return int16_t(st.octave * 12 + pc);
};

int16_t KeyboardMapper::mapScale(uint8_t id, const PerformanceState &st) const
{
    return mapKeyboard(id, st); // same 12 keys, flags differ
}

int16_t KeyboardMapper::mapChromatic(uint8_t id, const PerformanceState &st) const
{
    // Fold: distribute 16 pads over scale degrees across octaves
    // Build degree list from mask
    uint8_t deg[12];
    uint8_t k = 0;
    for (uint8_t pc = 0; pc < 12; ++pc)
        if (Scale::inMask(pc, st.scaleMask))
            deg[k++] = pc;
    if (k == 0)
        return -1;
    const uint8_t idx = id % k;
    const uint8_t oct = id / k;
    const uint8_t pc = deg[idx];
    return int16_t((st.octave + oct) * 12 + ((st.root + pc) % 12));
}

KeyMapResult KeyboardMapper::map(uint8_t id, const PerformanceState &st) const
{
    KeyMapResult r{.pitch = -1, .isScale = false, .isBlack = false};
    int16_t p = -1;
    switch (st.mode)
    {
    case PerformanceMode::Keyboard:
        p = mapKeyboard(id, st);
        break;
    case PerformanceMode::Scale:
        p = mapScale(id, st);
        break;
    case PerformanceMode::Chromatic:
        p = mapChromatic(id, st);
        break;
    case PerformanceMode::Chord:
        p = mapKeyboard(id, st);
        break; // stub
    }
    r.pitch = p;
    if (p >= 0)
    {
        const uint8_t pc = uint8_t((p - st.root + 12) % 12);
        r.isScale = Scale::inMask(pc, st.scaleMask);
        r.isBlack = isBlackPC(uint8_t(p % 12));
    }
    return r;
}