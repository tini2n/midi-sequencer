#pragma once
#include <stdint.h>
#include <vector>

struct CursorEvent
{
    uint8_t btnId; // 0..15 (2×8, row-major; bottom row 0..7, top row 8..15)
    bool down;     // true=press, false=release
};

class CursorInput
{
public:
    // Serial keyboard mapping:
    // bottom row: '1'..'8' => 0..7
    // top row:    'q'..'i' => 8..15
    void poll(std::vector<CursorEvent> &out);

private:
    bool held_[16] = {false};
};
