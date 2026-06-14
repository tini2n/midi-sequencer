#pragma once
#include <U8g2lib.h>
#include "../ui_ctx.hpp"
#include "../oled_renderer.hpp"

// Full-screen settings overlay (shown when CTL 6 / settingsMode is active).
// Displays BPM in a large font with encoder hints.
// All text is drawn into U8g2's 1-bit layer; it is composited onto the gray
// canvas at full brightness in OledRenderer::send().
// Header-only — small enough to not need a .cpp.
class SettingsView {
public:
    void draw(OledRenderer& oled, const UICtx& ctx) {
        U8G2& gfx = oled.gfx();
        // Title bar
        gfx.setFont(u8g2_font_5x7_tf);
        gfx.setDrawColor(6);
        gfx.drawBox(0, 0, 256, 9);
        gfx.setDrawColor(15);
        gfx.drawStr(2, 7, "SETTINGS");

        // BPM label
        gfx.setDrawColor(7);
        gfx.drawStr(2, 22, "BPM");

        // BPM value — large font
        char buf[8];
        snprintf(buf, sizeof(buf), "%.1f", (double)ctx.pat.tempo);
        gfx.setFont(u8g2_font_logisoso20_tf);
        gfx.setDrawColor(15);
        uint16_t w = gfx.getStrWidth(buf);
        gfx.drawStr((256 - w) / 2, 48, buf);   // centred, baseline at y=48

        // Encoder hints (dim)
        gfx.setFont(u8g2_font_5x7_tf);
        gfx.setDrawColor(5);
        gfx.drawStr(2, 58, "K1:turn=+/-0.5  K1:press=reset 120");
    }
};
