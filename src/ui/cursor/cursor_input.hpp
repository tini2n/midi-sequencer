#pragma once
#include <stdint.h>

struct CursorEvent
{
    uint8_t btn;
    bool down;
};

class CursorSerial
{
public:
    // bottom: '1'..'8' → btn 0..7, top: 'q'..'i' → btn 8..15; toggle down/up
    bool poll(CursorEvent &ev);

private:
    bool held_[16] = {false};
};
