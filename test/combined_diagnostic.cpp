// Combined SPI (SSD1322) + I2C (PCF8575) coexistence diagnostic.
//
// Wiring assumed:
//   SPI  — SCK=13  MOSI=11  MISO=12  CS=10  DC=9  RST=8
//   I2C  — SDA=18  SCL=19
//   PCF8575 at I2C address 0x20
//
// Flash: pio run -e teensy41_combined_test -t upload
//
// What to look for:
//   STEP 1: all bus pins should read HIGH. LOW before any init = hardware short.
//   STEP 4: SDA/SCL must stay HIGH after OLED init. If they change → wiring issue.
//   STEP 5: PCF scan after OLED. Timing race appears here.

#include <Arduino.h>
#include <Wire.h>
#include <SPI.h>
#include <U8g2lib.h>

// ── Pin map ────────────────────────────────────────────────────────────────
static constexpr uint8_t SDA_PIN  = 18;
static constexpr uint8_t SCL_PIN  = 19;
static constexpr uint8_t PCF_ADDR = 0x20;
// SPI / OLED
static constexpr uint8_t CS_PIN   = 10;
static constexpr uint8_t DC_PIN   =  9;
static constexpr uint8_t RST_PIN  =  8;
static constexpr uint8_t MOSI_PIN = 11;
static constexpr uint8_t MISO_PIN = 12;
static constexpr uint8_t SCK_PIN  = 13;

U8G2_SSD1322_NHD_256X64_F_4W_HW_SPI u8g2(U8G2_R0, CS_PIN, DC_PIN, RST_PIN);

// ── Helpers ────────────────────────────────────────────────────────────────
static void hr(const char* title) {
    Serial.printf("\n── %s\n", title);
}

// Read and print the logic state of every relevant pin.
// Call before Wire.begin() or after releasing the bus.
static void pinCensus(const char* label) {
    struct PinInfo { uint8_t pin; const char* name; };
    static const PinInfo pins[] = {
        {SDA_PIN,  "SDA  (I2C)"},
        {SCL_PIN,  "SCL  (I2C)"},
        {CS_PIN,   "CS   (OLED SPI)"},
        {DC_PIN,   "DC   (OLED SPI)"},
        {RST_PIN,  "RST  (OLED SPI)"},
        {MOSI_PIN, "MOSI (SPI)"},
        {MISO_PIN, "MISO (SPI)"},
        {SCK_PIN,  "SCK  (SPI)"},
    };
    Serial.printf("  [%s] Pin states (INPUT_PULLUP):\n", label);
    for (const auto& p : pins) {
        pinMode(p.pin, INPUT_PULLUP);
    }
    delayMicroseconds(200);
    for (const auto& p : pins) {
        bool hi = digitalRead(p.pin);
        // Flag unexpected LOW pins (something driving them down externally)
        Serial.printf("    Pin %2u %-20s %s\n", p.pin, p.name,
                      hi ? "HIGH" : "LOW  ← externally driven / short");
    }
}

// Detect whether any OLED SPI pin is shorted to SDA or SCL.
// Method: drive each SPI pin LOW briefly, check if SDA/SCL follows.
// Safe only before Wire.begin() and before OLED init.
static void shortDetect() {
    Serial.println("  [Short detect] Toggling each SPI pin LOW — watching SDA/SCL...");
    const uint8_t spiPins[] = {CS_PIN, DC_PIN, RST_PIN, MOSI_PIN, SCK_PIN};
    const char*   spiNames[]= {"CS","DC","RST","MOSI","SCK"};
    pinMode(SDA_PIN, INPUT_PULLUP);
    pinMode(SCL_PIN, INPUT_PULLUP);
    delayMicroseconds(100);
    bool anyShort = false;
    for (int i = 0; i < 5; i++) {
        pinMode(spiPins[i], OUTPUT);
        digitalWrite(spiPins[i], LOW);
        delayMicroseconds(50);
        bool sdaLow = !digitalRead(SDA_PIN);
        bool sclLow = !digitalRead(SCL_PIN);
        digitalWrite(spiPins[i], HIGH);
        pinMode(spiPins[i], INPUT_PULLUP);
        delayMicroseconds(50);
        if (sdaLow || sclLow) {
            anyShort = true;
            Serial.printf("    *** SHORT: %s (pin %u) pulls %s%s LOW!\n",
                          spiNames[i], spiPins[i],
                          sdaLow ? "SDA " : "",
                          sclLow ? "SCL"  : "");
        }
    }
    if (!anyShort)
        Serial.println("    No shorts detected between SPI and I2C pins. Good.");
}

static void busRecover() {
    Serial.println("  Running 9-clock I2C bus recovery...");
    pinMode(SDA_PIN, INPUT_PULLUP);
    pinMode(SCL_PIN, OUTPUT);
    delayMicroseconds(10);
    bool released = false;
    for (int i = 0; i < 9; i++) {
        digitalWrite(SCL_PIN, LOW);  delayMicroseconds(5);
        digitalWrite(SCL_PIN, HIGH); delayMicroseconds(5);
        if (digitalRead(SDA_PIN)) { released = true; break; }
    }
    pinMode(SDA_PIN, OUTPUT);
    digitalWrite(SDA_PIN, LOW);  delayMicroseconds(5);
    digitalWrite(SCL_PIN, HIGH); delayMicroseconds(5);
    digitalWrite(SDA_PIN, HIGH); delayMicroseconds(5);
    pinMode(SDA_PIN, INPUT_PULLUP);
    pinMode(SCL_PIN, INPUT_PULLUP);
    delayMicroseconds(200);
    Serial.printf("  SDA after recovery: %s\n",
                  digitalRead(SDA_PIN) ? "HIGH ✓" : "LOW  ✗ — hardware fault");
    if (!released)
        Serial.println("  WARNING: SDA never released during 9 pulses — check wiring.");
}

static int i2cScan(bool verbose = true) {
    int count = 0;
    for (uint8_t addr = 1; addr < 0x78; addr++) {
        Wire.beginTransmission(addr);
        if (Wire.endTransmission() == 0) {
            if (verbose)
                Serial.printf("  FOUND 0x%02X%s\n", addr,
                              addr == PCF_ADDR ? " ← PCF8575"
                                               : " ← UNKNOWN (not the PCF — check OLED I2C interface)");
            count++;
        }
    }
    if (count == 0 && verbose) Serial.println("  No devices found.");
    return count;
}

static bool pcfPing() {
    Wire.beginTransmission(PCF_ADDR);
    return Wire.endTransmission() == 0;
}

static bool pcfWrite(uint16_t val) {
    Wire.beginTransmission(PCF_ADDR);
    Wire.write(uint8_t(val & 0xFF));
    Wire.write(uint8_t(val >> 8));
    return Wire.endTransmission() == 0;
}

// ── setup ──────────────────────────────────────────────────────────────────
void setup() {
    Serial.begin(115200);
    while (!Serial && millis() < 3000);
    delay(300);

    Serial.println(F("\n╔═══════════════════════════════════════════════════╗"));
    Serial.println(F("║  SPI (SSD1322 OLED) + I2C (PCF8575) Diagnostic   ║"));
    Serial.println(F("╚═══════════════════════════════════════════════════╝\n"));

    // ── STEP 1: Pin census before any peripheral init ─────────────────────
    hr("STEP 1 — Pin census (no Wire, no SPI, no OLED)");
    pinCensus("before all init");

    // ── STEP 1b: SPI↔I2C short detection ─────────────────────────────────
    hr("STEP 1b — Short detection: SPI pins vs I2C pins");
    shortDetect();

    // ── STEP 2: Bus recovery + Wire init, PCF scan before OLED ───────────
    hr("STEP 2 — Wire.begin + I2C scan (OLED not yet initialised)");
    {
        // Only recover if SDA is actually stuck low
        pinMode(SDA_PIN, INPUT_PULLUP);
        delayMicroseconds(200);
        if (!digitalRead(SDA_PIN)) busRecover();
    }
    Wire.begin();
    Wire.setClock(400000);
    delay(10);
    Serial.println("  I2C scan:");
    int beforeCount = i2cScan();
    bool pcfBefore = pcfPing();
    Serial.printf("  PCF8575 ping: %s\n", pcfBefore ? "OK ✓" : "FAIL ✗");
    if (pcfBefore) {
        bool w = pcfWrite(0xFFFF);
        Serial.printf("  PCF write 0xFFFF: %s\n", w ? "OK ✓" : "FAIL ✗");
    }

    // ── STEP 3: Init OLED (SPI) ───────────────────────────────────────────
    hr("STEP 3 — u8g2.begin() — SPI OLED init");
    Serial.println("  OLED pin wiring used:");
    Serial.printf("    CS  = pin %u\n", CS_PIN);
    Serial.printf("    DC  = pin %u\n", DC_PIN);
    Serial.printf("    RST = pin %u\n", RST_PIN);
    Serial.printf("    MOSI= pin %u  (HW SPI0)\n", MOSI_PIN);
    Serial.printf("    SCK = pin %u  (HW SPI0)\n", SCK_PIN);
    Serial.printf("    SDA = pin %u  (I2C — must NOT be connected to OLED)\n", SDA_PIN);
    Serial.printf("    SCL = pin %u  (I2C — must NOT be connected to OLED)\n", SCL_PIN);

    u8g2.setBusClock(8000000);
    bool oledOk = u8g2.begin();
    Serial.printf("  u8g2.begin(): %s\n", oledOk ? "OK ✓" : "FAIL ✗");

    if (oledOk) {
        u8g2.clearBuffer();
        u8g2.setDrawColor(10);
        u8g2.drawBox(0, 0, 256, 64);
        u8g2.setFont(u8g2_font_5x7_tf);
        u8g2.setDrawColor(0);
        u8g2.drawStr(10, 20, "OLED + PCF Diagnostic");
        u8g2.drawStr(10, 36, "Check Serial Monitor");
        u8g2.sendBuffer();
        Serial.println("  Frame sent to OLED — should see text on screen.");
    }

    // ── STEP 4: Pin census AFTER OLED init ───────────────────────────────
    hr("STEP 4 — Pin census AFTER OLED init");
    Serial.println("  (SDA/SCL must still be HIGH — OLED uses SPI, not I2C)");
    pinCensus("after OLED init");

    // ── STEP 5: I2C scan AFTER OLED init ─────────────────────────────────
    hr("STEP 5 — I2C scan AFTER OLED init");
    Wire.begin();
    Wire.setClock(400000);
    Serial.println("  I2C scan:");
    int afterCount = i2cScan();
    bool pcfAfter = pcfPing();
    Serial.printf("  PCF8575 ping: %s\n", pcfAfter ? "OK ✓" : "FAIL ✗");

    // Analyse what changed
    Serial.println();
    if (afterCount > beforeCount) {
        bool unknownAppeared = false;
        for (uint8_t addr = 1; addr < 0x78; addr++) {
            if (addr == PCF_ADDR) continue;
            Wire.beginTransmission(addr);
            if (Wire.endTransmission() == 0) { unknownAppeared = true; break; }
        }
        if (unknownAppeared)
            Serial.println(F("  *** OLED placed a device on the I2C bus! Check BS0/BS1 resistors."));
        else
            Serial.println(F("  PCF appeared after OLED init delay — power-on timing race."));
    }
    if (pcfBefore && !pcfAfter)
        Serial.println(F("  *** PCF LOST after OLED init. Likely 3.3V rail voltage droop."));
    if (!pcfBefore && pcfAfter)
        Serial.println(F("  PCF appeared only after delay — power-on race. Firmware 600ms wait should fix this."));
    if (pcfBefore && pcfAfter)
        Serial.println(F("  OLED + PCF8575 both working — no conflict detected!"));
    if (!pcfBefore && !pcfAfter)
        Serial.println(F("  PCF8575 absent both before and after OLED init. Pure I2C / power problem."));

    // ── STEP 6: PCF write/read with OLED active ───────────────────────────
    if (pcfAfter) {
        hr("STEP 6 — PCF write/read while OLED is active");
        bool w = pcfWrite(0xFFFF);
        Serial.printf("  Write 0xFFFF: %s\n", w ? "OK ✓" : "FAIL ✗");
        uint16_t rb = 0;
        Wire.requestFrom(PCF_ADDR, (uint8_t)2);
        delay(2);
        if (Wire.available() >= 2) {
            rb = uint16_t(Wire.read()) | (uint16_t(Wire.read()) << 8);
            Serial.printf("  Read back: 0x%04X %s\n", rb,
                          rb == 0xFFFF ? "OK ✓" : "MISMATCH ✗ — PCF not holding state");
        } else {
            Serial.println("  Read: no data ✗");
        }

        // Interleave OLED frame + PCF read to check for crosstalk
        hr("STEP 6b — Interleave OLED send + PCF read");
        for (int round = 0; round < 3; round++) {
            // Trigger a full OLED frame transfer (heavy SPI burst)
            u8g2.clearBuffer();
            u8g2.setDrawColor(5);
            u8g2.drawBox(0, 0, 256, 64);
            u8g2.sendBuffer();
            // Immediately read PCF
            bool ok = pcfPing();
            Serial.printf("  Round %d: OLED frame sent — PCF ping: %s\n",
                          round + 1, ok ? "OK ✓" : "FAIL ✗");
            delay(50);
        }
    }

    hr("DONE");
    Serial.println(F("Commands:"));
    Serial.println(F("  'r' — re-run steps 4+5 (pin census + I2C scan, OLED stays on)"));
    Serial.println(F("  'p' — pin census only"));
    Serial.println(F("  's' — SPI↔I2C short detect"));
    Serial.println(F("  'c' — PCF continuous ping test (100 pings, report failures)"));
}

// ── loop ───────────────────────────────────────────────────────────────────
void loop() {
    if (!Serial.available()) return;
    char c = char(Serial.read());

    if (c == 'r') {
        hr("Re-run: pin census + I2C scan");
        pinCensus("now");
        Wire.begin(); Wire.setClock(400000);
        i2cScan();
        Serial.printf("PCF ping: %s\n", pcfPing() ? "OK" : "FAIL");
    }
    if (c == 'p') {
        pinCensus("now");
    }
    if (c == 's') {
        hr("Short detect");
        shortDetect();
    }
    if (c == 'c') {
        hr("PCF continuous ping (100 rounds)");
        Wire.begin(); Wire.setClock(400000);
        int ok = 0, fail = 0;
        for (int i = 0; i < 100; i++) {
            if (pcfPing()) ok++; else fail++;
            delay(10);
        }
        Serial.printf("  OK=%d  FAIL=%d  (%.0f%% success)\n",
                      ok, fail, 100.0f * ok / 100);
        if (fail > 0)
            Serial.println(F("  PCF is unstable — check 3.3V cap, I2C pull-ups, and power-on delay."));
    }
}
