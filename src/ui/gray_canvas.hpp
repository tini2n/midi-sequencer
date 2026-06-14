#pragma once
#include <stdint.h>
#include <string.h>

// 256×64 4-bit grayscale framebuffer for the SSD1322 (8 KB).
//
// Packing matches the panel and U8g2's SSD1322 driver: 2 pixels per byte,
// high nibble = left pixel (even x), low nibble = right pixel (odd x),
// row-major with STRIDE bytes per row.
//
// Unlike U8g2 (1 bit/pixel, every lit pixel = full brightness), this buffer
// stores a real 0–15 gray value per pixel, so notes can be shaded by velocity.
class GrayCanvas {
public:
    static constexpr int      W      = 256;
    static constexpr int      H      = 64;
    static constexpr int      STRIDE = W / 2;                  // 128 bytes/row
    static constexpr uint32_t BYTES  = uint32_t(STRIDE) * H;   // 8192

    void clear(uint8_t gray = 0) {
        memset(buf_, (gray & 0x0f) | uint8_t(gray << 4), BYTES);
    }

    inline void setPixel(int x, int y, uint8_t gray) {
        if ((unsigned)x >= (unsigned)W || (unsigned)y >= (unsigned)H) return;
        uint8_t* p = &buf_[y * STRIDE + (x >> 1)];
        if (x & 1) *p = (*p & 0xf0) | (gray & 0x0f);
        else       *p = (*p & 0x0f) | uint8_t((gray & 0x0f) << 4);
    }

    void fillRect(int x, int y, int w, int h, uint8_t gray) {
        if (w <= 0 || h <= 0) return;
        int x1 = x + w, y1 = y + h;
        if (x  < 0) x  = 0;
        if (y  < 0) y  = 0;
        if (x1 > W) x1 = W;
        if (y1 > H) y1 = H;
        for (int yy = y; yy < y1; ++yy)
            for (int xx = x; xx < x1; ++xx)
                setPixel(xx, yy, gray);
    }

    // Composite U8g2's 1-bit full buffer onto this canvas: every set bit -> `gray`.
    // U8g2 full-buffer layout: H/8 tile-rows, one byte per (x, tile-row), where the
    // byte is a vertical 8-pixel slice — bit (y & 7) is the pixel. Cleared bits are
    // left untouched so the gray content underneath shows through.
    void composite1bit(const uint8_t* u8g2buf, uint8_t gray = 15) {
        for (int tr = 0; tr < H / 8; ++tr) {
            const uint8_t* row = u8g2buf + tr * W;
            const int      y0  = tr * 8;
            for (int x = 0; x < W; ++x) {
                uint8_t b = row[x];
                while (b) {
                    int bit = __builtin_ctz(b);
                    setPixel(x, y0 + bit, gray);
                    b &= uint8_t(b - 1);
                }
            }
        }
    }

    const uint8_t* data() const { return buf_; }

private:
    uint8_t buf_[BYTES];
};
