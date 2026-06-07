#include <Arduino.h>
#include <Wire.h>

// ── config ────────────────────────────────────────────────────────────────────
static constexpr uint8_t PCF_ADDR = 0x20;
static constexpr uint8_t SDA_PIN  = 18;
static constexpr uint8_t SCL_PIN  = 19;

// ── low-level PCF helpers ─────────────────────────────────────────────────────
static bool pcfWrite(uint16_t val) {
    Wire.beginTransmission(PCF_ADDR);
    Wire.write((uint8_t)(val & 0xFF));
    Wire.write((uint8_t)(val >> 8));
    return Wire.endTransmission() == 0;
}

static bool pcfRead(uint16_t& val) {
    Wire.requestFrom(PCF_ADDR, (uint8_t)2);
    uint32_t t = millis();
    while (Wire.available() < 2) {
        if (millis() - t > 20) return false;
    }
    uint8_t lo = Wire.read();
    uint8_t hi = Wire.read();
    val = (uint16_t)lo | ((uint16_t)hi << 8);
    while (Wire.available()) Wire.read();
    return true;
}

static void hr(const char* title) {
    Serial.println();
    Serial.println(F("─────────────────────────────────────────────"));
    Serial.printf ("  %s\n", title);
    Serial.println(F("─────────────────────────────────────────────"));
}

static void printBits(uint16_t v) {
    for (int i = 15; i >= 0; i--) {
        Serial.print((v >> i) & 1);
        if (i == 8) Serial.print(' ');
    }
}

// ── TEST 1: SDA/SCL idle state ────────────────────────────────────────────────
// Checks pull-ups are present BEFORE Wire takes control of the pins.
// If a line reads LOW here → short to GND or missing pull-up resistor.
static void testBusIdle() {
    hr("TEST 1 — SDA/SCL idle state (before Wire.begin)");

    pinMode(SDA_PIN, INPUT);
    pinMode(SCL_PIN, INPUT);
    delayMicroseconds(200);

    bool sda = digitalRead(SDA_PIN);
    bool scl = digitalRead(SCL_PIN);

    Serial.printf("  SDA pin %u: %s\n", SDA_PIN, sda ? "HIGH  ✓" : "LOW   ✗  ← stuck");
    Serial.printf("  SCL pin %u: %s\n", SCL_PIN, scl ? "HIGH  ✓" : "LOW   ✗  ← stuck");

    if (sda && scl) {
        Serial.println(F("\n  PASS — both lines idle HIGH"));
        Serial.println(F("  Pull-up resistors are working on the I2C bus."));
    } else if (!sda && !scl) {
        Serial.println(F("\n  FAIL — BOTH lines stuck LOW"));
        Serial.println(F("  Possible causes:"));
        Serial.println(F("    • Short to GND on SDA or SCL"));
        Serial.println(F("    • Pull-up resistors missing or wrong value"));
        Serial.println(F("    • Teensy I2C pins damaged"));
    } else {
        Serial.printf(F("\n  FAIL — %s stuck LOW\n"), (!sda) ? "SDA" : "SCL");
        Serial.println(F("  Check wiring and pull-up on that line."));
    }
}

// ── TEST 2: I2C bus scan ──────────────────────────────────────────────────────
static bool testBusScan() {
    hr("TEST 2 — I2C bus scan (0x01–0x77)");

    int count = 0;
    bool pcfFound = false;

    for (uint8_t addr = 1; addr < 0x78; addr++) {
        Wire.beginTransmission(addr);
        uint8_t err = Wire.endTransmission();
        if (err == 0) {
            Serial.printf("  [FOUND] 0x%02X", addr);
            if (addr == PCF_ADDR) { Serial.print(F("  ← PCF8575")); pcfFound = true; }
            Serial.println();
            count++;
        }
    }

    Serial.println();
    if (count == 0) {
        Serial.println(F("  FAIL — no devices found on I2C bus."));
        Serial.println(F("  Check: SDA/SCL wiring, pull-up resistors, VCC/GND to PCF."));
    } else {
        Serial.printf("  Found %d device(s).\n", count);
        Serial.println(pcfFound ? F("  PCF8575 at 0x20 confirmed.") : F("  WARNING: PCF8575 not at 0x20."));
    }
    return pcfFound;
}

// ── TEST 3: PCF write/read patterns ──────────────────────────────────────────
static void testPcfDrive() {
    hr("TEST 3 — PCF8575 write/read patterns");

    struct { uint16_t val; const char* label; } cases[] = {
        { 0xFFFF, "all HIGH  (input / weak pull-up)" },
        { 0x0000, "all LOW   (output driven low)"    },
        { 0xAAAA, "hi-byte=AA  alternating 10101010" },
        { 0x5555, "lo-byte=55  alternating 01010101" },
        { 0x00FF, "lo-byte=00  hi-byte=FF"           },
        { 0xFF00, "lo-byte=FF  hi-byte=00"           },
    };

    for (auto& c : cases) {
        if (!pcfWrite(c.val)) {
            Serial.printf("  0x%04X  WRITE FAILED — I2C error\n", c.val);
            continue;
        }
        delayMicroseconds(200);
        uint16_t rb = 0xBEEF;
        if (!pcfRead(rb)) {
            Serial.printf("  0x%04X  READ FAILED  — I2C error\n", c.val);
            continue;
        }
        bool ok = (rb == c.val);
        Serial.printf("  write=0x%04X  read=0x%04X  %s\n",
                      c.val, rb, ok ? "PASS" : "FAIL ← mismatch");
        if (!ok) {
            Serial.printf("         stuck-HIGH bits: 0x%04X\n", rb & ~c.val);
            Serial.printf("         stuck-LOW  bits: 0x%04X\n", ~rb & c.val);
        }
        Serial.printf("         bits: "); printBits(rb); Serial.println();
    }

    pcfWrite(0xFFFF);
}

// ── TEST 4: pin walk — drive one LOW at a time ────────────────────────────────
static void testPinWalk() {
    hr("TEST 4 — pin walk (one LOW walks P00→P15)");
    Serial.println(F("  Each iteration: one pin driven LOW, all others HIGH."));
    Serial.println(F("  Expected: readback == written value exactly."));
    Serial.println();
    Serial.println(F("  Pin  Written   Read      Match?"));
    Serial.println(F("  ───  ────────  ────────  ───────"));

    bool allPass = true;
    for (int i = 0; i < 16; i++) {
        uint16_t val = (uint16_t)(~(1u << i));
        if (!pcfWrite(val)) {
            Serial.printf("  P%02d  WRITE FAILED\n", i);
            allPass = false;
            continue;
        }
        delayMicroseconds(200);
        uint16_t rb = 0;
        if (!pcfRead(rb)) {
            Serial.printf("  P%02d  READ FAILED\n", i);
            allPass = false;
            continue;
        }
        bool ok = (rb == val);
        if (!ok) allPass = false;
        Serial.printf("  P%02d  0x%04X    0x%04X    %s\n",
                      i, val, rb, ok ? "PASS" : "FAIL");
    }

    Serial.println();
    Serial.println(allPass ? F("  ALL 16 PINS PASS") : F("  SOME PINS FAILED — chip I/O damaged"));
    pcfWrite(0xFFFF);
}

// ── TEST 5: I2C speed ─────────────────────────────────────────────────────────
static void testTiming() {
    hr("TEST 5 — I2C round-trip timing");
    Serial.println(F("  100 write+read cycles at 400 kHz."));
    Serial.println(F("  Expected: ~130–200 µs per cycle on a healthy bus."));
    Serial.println();

    uint16_t dummy;
    uint32_t t0 = micros();
    for (int i = 0; i < 100; i++) {
        pcfWrite(0xFFFF);
        pcfRead(dummy);
    }
    uint32_t us = micros() - t0;
    float perCycle = us / 100.0f;

    Serial.printf("  Total: %lu µs  |  Per cycle: %.1f µs\n", us, perCycle);
    if (perCycle < 500)
        Serial.println(F("  PASS — timing normal"));
    else
        Serial.println(F("  SLOW — high bus capacitance or pull-ups too weak"));
}

// ── RECOVERY: 9-clock flush ───────────────────────────────────────────────────
// Slave mid-transaction holds SDA low when master resets. Clock 9 SCL pulses to
// flush any in-progress byte, then send a STOP condition to release the bus.
// Call this BEFORE Wire.begin().
static void recoverBus() {
    hr("RECOVERY — clocking 9 SCL pulses to release stuck slave");
    pinMode(SDA_PIN, INPUT_PULLUP);
    pinMode(SCL_PIN, OUTPUT);
    delayMicroseconds(10);
    bool released = false;
    for (int i = 0; i < 9; i++) {
        digitalWrite(SCL_PIN, LOW);  delayMicroseconds(5);
        digitalWrite(SCL_PIN, HIGH); delayMicroseconds(5);
        if (digitalRead(SDA_PIN)) { released = true; break; }
    }
    // STOP condition
    pinMode(SDA_PIN, OUTPUT);
    digitalWrite(SDA_PIN, LOW);  delayMicroseconds(5);
    digitalWrite(SCL_PIN, HIGH); delayMicroseconds(5);
    digitalWrite(SDA_PIN, HIGH); delayMicroseconds(5);
    pinMode(SDA_PIN, INPUT_PULLUP);
    pinMode(SCL_PIN, INPUT_PULLUP);
    delayMicroseconds(100);
    Serial.printf("  SDA after recovery: %s\n", digitalRead(SDA_PIN) ? "HIGH ✓" : "LOW  ✗");
    Serial.println(released ? "  Slave released SDA during clocking — bus should be free."
                            : "  SDA still LOW after 9 pulses — check for short to GND.");
}

// ── TEST 6: bus lock-up check ─────────────────────────────────────────────────
static void testBusLockup() {
    hr("TEST 6 — bus lock-up check (SDA state after init)");

    // Temporarily release Wire to sample the pin
    // (We can't do this cleanly without Wire.end(), so just report Wire status)
    // Check if a simple transaction completes without timeout
    uint32_t t0 = millis();
    Wire.beginTransmission(PCF_ADDR);
    uint8_t err = Wire.endTransmission();
    uint32_t dt = millis() - t0;

    if (err == 0 && dt < 5) {
        Serial.println(F("  PASS — bus responding, no lock-up detected"));
        Serial.printf("  Transaction time: %lu ms\n", dt);
    } else if (dt >= 5) {
        Serial.printf("  WARN — slow response: %lu ms (bus may be stretched or contended)\n", dt);
    } else {
        Serial.printf("  FAIL — error code %u\n", err);
        Serial.println(F("  Possible lock-up. Try power cycling both Teensy and PCF."));
    }
}

// ── setup / loop ──────────────────────────────────────────────────────────────

static bool s_pcfPresent = false;

void setup() {
    Serial.begin(115200);
    while (!Serial && millis() < 4000);
    delay(200);

    Serial.println(F("\n═══════════════════════════════════════════════"));
    Serial.println(F("  Teensy 4.1 + PCF8575  —  I2C Full Diagnostic"));
    Serial.println(F("═══════════════════════════════════════════════"));
    Serial.printf ("  PCF target address : 0x%02X\n", PCF_ADDR);
    Serial.printf ("  SDA                : pin %u\n", SDA_PIN);
    Serial.printf ("  SCL                : pin %u\n", SCL_PIN);
    Serial.println(F("  I2C speed          : 400 kHz\n"));

    // Test 1 must run BEFORE Wire.begin
    testBusIdle();

    // Always run recovery first — harmless if bus is fine, essential if SDA is stuck
    recoverBus();

    Wire.begin();
    Wire.setClock(400000);
    delay(10);

    s_pcfPresent = testBusScan();

    if (s_pcfPresent) {
        testPcfDrive();
        testPinWalk();
        testTiming();
    } else {
        Serial.println(F("\n  Skipping PCF tests — device not found."));
    }

    testBusLockup();

    hr("DONE — interactive commands");
    Serial.println(F("  0 = bus recovery    1 = bus scan        2 = drive test"));
    Serial.println(F("  3 = pin walk        4 = timing          5 = lock-up check"));
    Serial.println(F("  r = run all tests"));
}

void loop() {
    if (!Serial.available()) return;
    char c = (char)Serial.read();
    switch (c) {
        case '0': recoverBus(); Wire.begin(); Wire.setClock(400000);
                  s_pcfPresent = testBusScan(); break;
        case '1': testBusScan();    break;
        case '2': if (s_pcfPresent) testPcfDrive();  else Serial.println(F("PCF not found")); break;
        case '3': if (s_pcfPresent) testPinWalk();   else Serial.println(F("PCF not found")); break;
        case '4': if (s_pcfPresent) testTiming();    else Serial.println(F("PCF not found")); break;
        case '5': testBusLockup();  break;
        case 'r':
            s_pcfPresent = testBusScan();
            if (s_pcfPresent) { testPcfDrive(); testPinWalk(); testTiming(); }
            testBusLockup();
            break;
        default: break;
    }
}
