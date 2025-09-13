#include <Arduino.h>
#include "cursor_input.hpp"

bool CursorSerial::poll(CursorEvent &ev)
{
    if (!Serial.available())
        return false;

    char c = Serial.read();
    int id = -1;

    if (c >= '1' && c <= '8')
        id = c - '1';
    else if (c >= 'q' && c <= 'i')
        id = 8 + (c - 'q');
    else
        return false;

    held_[id] = !held_[id];
    ev = {(uint8_t)id, held_[id]};
    
    return true;
}
