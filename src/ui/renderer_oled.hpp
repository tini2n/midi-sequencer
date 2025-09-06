#pragma once
#include <U8g2lib.h>
#include "model/pattern.hpp"
#include "model/viewport.hpp"

class OledRenderer
{
public:
    bool begin();
    uint32_t drawFrame(const Pattern &, const Viewport &, uint32_t microsNow, const char* hud=nullptr);

private:
    U8G2_SSD1306_128X64_NONAME_F_HW_I2C u8g2_{U8G2_R0, U8X8_PIN_NONE};
    void drawGrid(const Viewport &);
};
