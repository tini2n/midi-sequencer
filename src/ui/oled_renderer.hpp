#pragma once
#include <U8g2lib.h>
#include <SPI.h>

// Thin wrapper around U8g2 for the SSD1322 256×64 4-bit grayscale display.
//
// Uses full-buffer mode (F): 8 KB framebuffer held in RAM1.
// Workflow per frame: clear() → draw everything → send().
// send() transfers the full 8 KB over SPI once (~5 ms @ 16 MHz, ~3 ms @ 24 MHz).
//
// Gray levels: setDrawColor(0) = off, setDrawColor(15) = full brightness.
// velToGray() maps MIDI velocity 0–127 to gray 4–15 so even soft notes are visible.
class OledRenderer {
public:
    bool begin() {
        u8g2_.setBusClock(8000000);    // 8 MHz SPI
        bool ok = u8g2_.begin();
        u8g2_.setContrast(200);        // 0–255; 200 is bright without bloom
        return ok;
    }

    void clear() { u8g2_.clearBuffer(); }
    void send()  { u8g2_.sendBuffer(); }

    // Direct access for views — they set draw color and call U8g2 primitives.
    U8G2& gfx() { return u8g2_; }

    // MIDI velocity 0–127 → gray level 4–15.
    // Gray 4 = barely visible (pp), 15 = full brightness (ff).
    static uint8_t velToGray(uint8_t vel) {
        return uint8_t(4 + (uint16_t(vel) * 11u) / 127u);
    }

private:
    // SSD1322 NHD 256×64, 4-bit grayscale, 4-wire HW SPI.
    // Teensy 4.1 HW SPI: SCK=13, MOSI=11 (fixed).
    // CS=10, DC=9, RST=8 (configured here).
    U8G2_SSD1322_NHD_256X64_F_4W_HW_SPI u8g2_{U8G2_R0, /*cs=*/10, /*dc=*/9, /*rst=*/8};
};
