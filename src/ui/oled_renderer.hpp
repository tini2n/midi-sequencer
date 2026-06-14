#pragma once
#include <U8g2lib.h>
#include <SPI.h>
#include <EventResponder.h>
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
// The 8 KB blit is sent over SPI by DMA (asynchronous): send() starts the
// transfer and returns immediately, so the ~8 ms transfer overlaps the main loop
// (MIDI/input keep being serviced) instead of blocking it. CS is raised and the
// SPI transaction closed in the DMA-completion ISR. The next frame's clear()
// waits for any in-flight transfer before touching the framebuffer — in practice
// a no-op, since frames are >30 ms apart and the DMA finishes within ~8 ms.
//
// U8g2 and the blit share the same HW SPI bus + DC/CS pins but never run
// concurrently: after begin() U8g2 never touches the bus again.
class OledRenderer {
public:
    bool begin() {
        self_ = this;
        dmaEvent_.attachImmediate(&OledRenderer::onDmaDone);  // fires in the DMA ISR
        u8g2_.setBusClock(kSpiHz);
        bool ok = u8g2_.begin();   // SSD1322 init + SPI.begin() + blank panel
        u8g2_.setContrast(200);    // global brightness, 0–255
        gray_.clear(0);
        return ok;
    }

    void clear() {
        waitDma();                 // previous frame must finish before we modify the buffer
        gray_.clear(0);
        u8g2_.clearBuffer();
    }

    void send() {
        gray_.composite1bit(u8g2_.getBufferPtr(), 15);  // text/labels on top, full bright
        startBlit();               // async DMA; returns immediately
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

    void waitDma() { while (dmaBusy_) yield(); }

    void startBlit() {
        SPI.beginTransaction(SPISettings(kSpiHz, MSBFIRST, SPI_MODE0));
        digitalWriteFast(PIN_CS, LOW);
        cmd(0x15); dat(COL_START); dat(COL_END);   // set column window
        cmd(0x75); dat(ROW_START); dat(ROW_END);   // set row window
        cmd(0x5C);                                 // write RAM
        digitalWriteFast(PIN_DC, HIGH);
        dmaBusy_ = true;
        // Async DMA, write-only (no RX buffer). The library handles cache for the
        // source buffer; completion fires onDmaDone(). Falls back to blocking if it
        // can't start, so the frame is never silently dropped.
        if (!SPI.transfer(gray_.data(), nullptr, GrayCanvas::BYTES, dmaEvent_)) {
            SPI.transfer(gray_.data(), nullptr, GrayCanvas::BYTES);  // sync fallback
            digitalWriteFast(PIN_CS, HIGH);
            SPI.endTransaction();
            dmaBusy_ = false;
        }
    }

    static void onDmaDone(EventResponderRef) {
        digitalWriteFast(PIN_CS, HIGH);
        SPI.endTransaction();
        self_->dmaBusy_ = false;
    }

    U8G2_SSD1322_NHD_256X64_F_4W_HW_SPI u8g2_{U8G2_R0, /*cs=*/10, /*dc=*/9, /*rst=*/8};
    GrayCanvas      gray_;
    EventResponder  dmaEvent_;
    volatile bool   dmaBusy_{false};
    inline static OledRenderer* self_{nullptr};
};
