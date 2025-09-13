#include <Arduino.h>
#include "cursor_input.hpp"

static int punctToIndex(char c)
{ // '!'..'*' => 0..7 on US layout
    const char *P = "!@#$%^&*";
    for (int i = 0; i < 8; i++)
        if (c == P[i])
            return i;
    return -1;
}

int CursorSerial::mapDown(char c)
{
    if (c >= '1' && c <= '8')
        return c - '1';
    if (c >= 'q' && c <= 'i')
        return 8 + (c - 'q');
    return -1;
}

int CursorSerial::mapUp(char c)
{
    if (c >= 'Q' && c <= 'I')
        return 8 + (c - 'Q');
    int k = punctToIndex(c);
    return k; // !@#$%^&* release 0..7
}

bool CursorSerial::poll(CursorEvent &ev)
{
    if (!Serial.available())
        return false;

    char c = Serial.read();

    int id = mapDown(c);
    if (id >= 0)
    {
        if (!held_[id])
        {
            held_[id] = true;
            ev = {(uint8_t)id, true};
            return true;
        } // ignore repeats
        return false;
    }

    id = mapUp(c);
    if (id >= 0)
    {
        if (held_[id])
        {
            held_[id] = false;
            ev = {(uint8_t)id, false};
            return true;
        }
        return false;
    }
    
    return false;
}
