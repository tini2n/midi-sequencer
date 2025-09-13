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

    void reset()
    {
        for (bool &h : held_)
            h = false;
    }

private:
    bool held_[16] = {false};
    static int mapDown(char c); // '1'..'8' -> 0..7, 'q'..'i' -> 8..15
    static int mapUp(char c);   // '!'..'*' -> 0..7, 'Q'..'I' -> 8..15
};
