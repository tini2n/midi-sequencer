#pragma once
#include <bitset>
#include <stdint.h>

// Lightweight step-based track for cursor sequencer mode.
namespace step {

struct Track {
    std::bitset<128> steps; // 8 pages × 16 steps
    uint8_t channel{1};
    uint8_t pitch{60};     // C4
    uint8_t page{0};       // 0..7 (page*16 + col)
    uint16_t debounce_us{3000};

    // Edge detection helpers for up to 32 buttons
    bool btnLast[32]{};
    bool edge(uint8_t idx, bool down) {
        bool e = (btnLast[idx] != down);
        btnLast[idx] = down;
        return e && down; // rising edge only
    }
};

} // namespace step
