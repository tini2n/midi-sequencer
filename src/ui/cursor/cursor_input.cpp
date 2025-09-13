#include <Arduino.h>
#include "cursor_input.hpp"

static int keyToBtn(char c)
{
    switch (c)
    {
    // 1..8 -> 0..7
    case '1':
        return 0;
    case '2':
        return 1;
    case '3':
        return 2;
    case '4':
        return 3;
    case '5':
        return 4;
    case '6':
        return 5;
    case '7':
        return 6;
    case '8':
        return 7;
    // qwertyui -> 8..15
    case 'q':
    case 'Q':
        return 8;
    case 'w':
    case 'W':
        return 9;
    case 'e':
    case 'E':
        return 10;
    case 'r':
    case 'R':
        return 11;
    case 't':
    case 'T':
        return 12;
    case 'y':
    case 'Y':
        return 13;
    case 'u':
    case 'U':
        return 14;
    case 'i':
    case 'I':
        return 15;
    default:
        return -1;
    }
}

void CursorInput::poll(std::vector<CursorEvent> &out)
{
    while (Serial.available())
    {
        char c = (char)Serial.read();
        Serial.printf("KBD: '%c' (0x%02X)\n", c, (uint8_t)c);

        if (c == ';')
        {
            for (uint8_t i = 0; i < 16; i++)
                if (held_[i])
                {
                    held_[i] = false;
                    out.push_back({i, false});
                }
            continue;
        }

        int id = keyToBtn(c);

        if (id < 0)
            continue;

        bool was = held_[id];
        held_[id] = !was;

        out.push_back({(uint8_t)id, !was});
    }
}
