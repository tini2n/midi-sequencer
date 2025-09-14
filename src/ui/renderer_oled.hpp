#pragma once
#include <U8g2lib.h>

#include "model/pattern.hpp"
#include "model/viewport.hpp"

#include "./widgets/piano_roll.hpp"

class OledRenderer
{
    PianoRoll pianoRoll_;

public:
    bool begin();
    uint32_t drawFrame(const Pattern &, const Viewport &, uint32_t microsNow, uint32_t playTick = 0, const char *hud = nullptr);
    
    public: void rollSetOptions(const PianoRoll::Options& o){ pianoRoll_.setOptions(o); }

private:
    U8G2_SSD1306_128X64_NONAME_F_HW_I2C u8g2_{U8G2_R0, U8X8_PIN_NONE};
};
