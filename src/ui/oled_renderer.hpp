#pragma once
#include <U8g2lib.h>
#include <SPI.h>
#include "gray_canvas.hpp"

// Renderer for the SSD1322 NHD 256×64 4-bit grayscale display.
//
// U8g2 is a 1-bit library: its SSD1322 driver writes every lit pixel at full
// brightness, so it alone cannot shade notes by velocity. To get real per-pixel
// grayscale we keep our own 8 KB nibble framebuffer (GrayCanvas) and blit it to
// the panel ourselves, while still using U8g2 for two things it does well:
//   1. Panel bring-up — u8g2_.begin() runs the proven SSD1322 init sequence.
//   2. Text/labels — drawn into U8g2's offscreen 1-bit buffer, then composited
//      onto the gray canvas at full brightness.
//
// Per-frame workflow (see ScreenManager):
//   clear()  → wipe gray canvas + U8g2 buffer
//   <views>  → notes/grid/playhead drawn as gray via gray(); text via gfx()
//   send()   → composite U8g2's 1-bit layer onto the gray canvas, blit 8 KB SPI
//
// U8g2 and the raw blit share the same HW SPI bus + DC/CS pins but never run
// concurrently: after begin() U8g2 never touches the bus again (we never call
// u8g2_.sendBuffer()), so the blit owns all subsequent transfers.
class OledRenderer {
public:
    bool begin() {
        u8g2_.setBusClock(kSpiHz);
        bool ok = u8g2_.begin();   // SSD1322 init + SPI.begin() + blank panel
        u8g2_.setContrast(200);    // global brightness, 0–255
        gray_.clear(0);
        return ok;
    }

    void clear() {
        gray_.clear(0);
        u8g2_.clearBuffer();
    }

    void send() {
        gray_.composite1bit(u8g2_.getBufferPtr(), 15);  // text/labels on top, full bright
        blit();
    }

    U8G2&       gfx()  { return u8g2_; }   // offscreen 1-bit canvas (text, labels)
    GrayCanvas& gray() { return gray_; }   // direct nibble framebuffer (notes, grid)

    void setContrast(uint8_t c) { u8g2_.setContrast(c); }

private:
    // HW SPI: SCK=13, MOSI=11 (fixed). CS=10, DC=9, RST=8 (match U8g2 ctor below).
    static constexpr uint8_t  PIN_DC = 9, PIN_CS = 10;
    static constexpr uint32_t kSpiHz = 8000000;
    // SSD1322 addresses 4 px per column; the NHD 256-wide window spans 0x1C..0x5B.
    static constexpr uint8_t COL_START = 0x1C, COL_END = 0x5B;
    static constexpr uint8_t ROW_START = 0x00, ROW_END = 0x3F;

    void cmd(uint8_t c) { digitalWriteFast(PIN_DC, LOW);  SPI.transfer(c); }
    void dat(uint8_t d) { digitalWriteFast(PIN_DC, HIGH); SPI.transfer(d); }

    void blit() {
        SPI.beginTransaction(SPISettings(kSpiHz, MSBFIRST, SPI_MODE0));
        digitalWriteFast(PIN_CS, LOW);
        cmd(0x15); dat(COL_START); dat(COL_END);   // set column window
        cmd(0x75); dat(ROW_START); dat(ROW_END);   // set row window
        cmd(0x5C);                                 // write RAM
        digitalWriteFast(PIN_DC, HIGH);
        SPI.transfer(gray_.data(), nullptr, GrayCanvas::BYTES);  // blit, no clobber
        digitalWriteFast(PIN_CS, HIGH);
        SPI.endTransaction();
    }

    U8G2_SSD1322_NHD_256X64_F_4W_HW_SPI u8g2_{U8G2_R0, /*cs=*/10, /*dc=*/9, /*rst=*/8};
    GrayCanvas gray_;
};
