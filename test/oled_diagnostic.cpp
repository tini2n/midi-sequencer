// OLED diagnostic sketch — run this STANDALONE before the full firmware.
// Tests the SSD1322 display in isolation (no encoders, no keyboard, no MIDI).
//
// How to flash:
//   pio run -e teensy41_oled_test -t upload
//
// What to check in the serial monitor:
//   1. "SPI begin OK"   → SPI is up
//   2. "Display begin OK/FAIL"  → U8g2 init result
//   3. "Frame N sent"   → sendBuffer() is calling successfully
//   If you see frames but the display is blank, the init sequence is wrong
//   for your specific module — try the alternative variants below.
//
// Wiring (Teensy 4.1 → SSD1322):
//   Pin 11 (MOSI)   → SDIN (data in)
//   Pin 13 (SCK)    → SCLK (clock)
//   Pin 10          → /CS  (chip select, active LOW)
//   Pin  9          → D/C  (data=HIGH, command=LOW)
//   Pin  8          → /RES (reset, active LOW)
//   3.3V            → VDD / VCC (check module — some need 5V for OLED boost)
//   GND             → GND
//
// NOTE: pin 12 (MISO) is NOT connected to the display — SSD1322 is write-only.
//       If encoders are also wired, disconnect pin 12 from K2 SW during this test.

#include <Arduino.h>
#include <SPI.h>
#include <U8g2lib.h>

// ─── Display variant — uncomment ONE line ─────────────────────────────────
// Try them in order if the screen stays blank.

// Option 1: Newhaven Display NHD-3.12-25664UCY2 (most common 256×64 SSD1322)
U8G2_SSD1322_NHD_256X64_F_4W_HW_SPI u8g2(U8G2_R0, /*cs=*/10, /*dc=*/9, /*rst=*/8);

// Option 2: NHD with alternate init (try if Option 1 shows nothing)
// U8G2_SSD1322_NHD_256X64_F_4W_HW_SPI u8g2(U8G2_R2, /*cs=*/10, /*dc=*/9, /*rst=*/8);

// Option 3: ER-OLEDM032 (WEX) module variant — 256×64
// U8G2_SSD1322_NHD_128X64_F_4W_HW_SPI u8g2(U8G2_R0, /*cs=*/10, /*dc=*/9, /*rst=*/8);
// ──────────────────────────────────────────────────────────────────────────

static constexpr uint8_t  CS_PIN  = 10;
static constexpr uint8_t  DC_PIN  =  9;
static constexpr uint8_t  RST_PIN =  8;
static constexpr uint32_t SPI_CLK = 4000000;   // 4 MHz — conservative; increase if OK

static uint16_t frame = 0;

void setup() {
    Serial.begin(115200);
    delay(1500);  // wait for serial monitor
    Serial.println("\n=== OLED Diagnostic ===");

    // Confirm pin states before init
    Serial.printf("CS=%u  DC=%u  RST=%u  MOSI=11  SCK=13\n", CS_PIN, DC_PIN, RST_PIN);

    // Drive RST manually to ensure clean reset
    pinMode(RST_PIN, OUTPUT);
    digitalWrite(RST_PIN, LOW);
    delay(10);
    digitalWrite(RST_PIN, HIGH);
    delay(10);
    Serial.println("RST pulse done");

    // Init SPI explicitly before U8g2 to check SPI bus health
    SPI.begin();
    Serial.println("SPI begin OK");

    // U8g2 init
    u8g2.setBusClock(SPI_CLK);
    bool ok = u8g2.begin();
    Serial.printf("Display begin: %s\n", ok ? "OK" : "FAIL (check wiring)");
    Serial.printf("SPI clock: %lu Hz\n", (unsigned long)SPI_CLK);

    // Draw a full white frame immediately
    u8g2.clearBuffer();
    u8g2.setDrawColor(15);
    u8g2.drawBox(0, 0, 256, 64);   // full white — most visible possible signal
    u8g2.sendBuffer();
    Serial.println("Frame 0 sent: full white — should see bright screen");
    delay(1000);

    // Draw checkerboard to distinguish from backlight glow
    u8g2.clearBuffer();
    for (int y = 0; y < 64; y++)
        for (int x = 0; x < 256; x++)
            if ((x + y) % 2 == 0)
                u8g2.drawPixel(x, y);
    u8g2.sendBuffer();
    Serial.println("Frame 1 sent: checkerboard");
    delay(1000);
}

void loop() {
    frame++;

    // Alternating gray levels — verifies grayscale is working
    uint8_t gray = (frame % 15) + 1;
    u8g2.clearBuffer();
    u8g2.setDrawColor(gray);
    u8g2.drawBox(0, 0, 256, 64);

    // Text overlay
    u8g2.setDrawColor(gray > 7 ? 0 : 15);  // contrast text
    u8g2.setFont(u8g2_font_5x7_tf);
    char buf[32];
    snprintf(buf, sizeof(buf), "gray=%u  frame=%u", gray, frame);
    u8g2.drawStr(4, 36, buf);

    u8g2.sendBuffer();
    Serial.printf("Frame %u: gray=%u\n", frame, gray);
    delay(500);
}
