# OLED (SSD1322) + PCF8575 Coexistence — Diagnosis & Fixes

Hardware: Teensy 4.1, SSD1322 NHD 256×64 OLED (SPI), PCF8575 I2C 16-bit I/O expander (matrix keyboard).

---

## Confirmed Root Cause — Power-on Timing Race

**Diagnostic result (2026-06-07):**

- Step 1: all pins HIGH — no wiring shorts.
- Step 1b: no SPI↔I2C shorts.
- Step 5 (I2C scan after OLED init): PCF not found.
- Continuous ping test `c` (run several seconds after boot): **100 / 100 OK**.

**Conclusion:** the PCF8575 is electrically healthy and stable. It simply needs more time after power-on before it starts responding to I2C. The firmware's I2C scan was running before the chip had finished booting.

The OLED module does not interfere with I2C in any way — no wrong interface mode, no voltage droop, no signal coupling.

---

## Fix (Already Applied)

In `src/app.cpp`, before `kb_.begin()`:

```cpp
// PCF8575 needs ~450ms to stabilise on power-on.
// The OLED init already occupies most of this, but pad to 600ms to be safe.
while (millis() < 600) {}
MatrixKB::Config kbCfg;
kbCfg.address = cfg::PCF_ADDRESS;
kb_.begin(kbCfg);
```

This is all that is needed. The PCF8575 is fully stable once it has had enough time to boot.

---

## Why the OLED Appeared to Cause the Problem

The OLED module was being powered from the same 3.3 V rail. When the OLED was disconnected, its SPI init sequence was skipped, and the Teensy reached the I2C scan slightly later — enough extra milliseconds for the PCF to be ready. When the OLED was connected, `u8g2.begin()` runs first and (ironically) its reset pulse sequence takes ~100–200 ms, which usually isn't enough. The symptom looked like OLED interference but was purely a timing coincidence.

---

## SSD1322 Interface Mode Reference

The SSD1322 chip selects its host interface via BS0, BS1, BS2 hardware pins. On NHD modules these are configured by SMD resistors (R5 / R8 area).

| BS2 | BS1 | BS0 | Interface      | Notes                                        |
|-----|-----|-----|----------------|----------------------------------------------|
|  1  |  0  |  0  | **4-wire SPI** | CS, SCLK, SDIN, D/C — **current mode**       |
|  1  |  0  |  1  | 3-wire SPI     | CS, SCLK, SDIN; D/C embedded in data stream  |
|  0  |  1  |  1  | 68xx parallel  | 6800-series 8-bit parallel bus               |
|  0  |  1  |  0  | 80xx parallel  | 8080-series 8-bit parallel bus               |

> Exact BS combinations vary by module manufacturer. Always check the datasheet for your specific board.

**R5 / R8 resistors** on NHD modules set BS0 and BS1. Current configuration (SPI4 mode) is correct — verified: no unknown I2C devices appear after OLED init.

---

## Full Pin Map

| Signal | Teensy Pin | Connected to      |
|--------|-----------|-------------------|
| SDA    | 18        | PCF8575 SDA       |
| SCL    | 19        | PCF8575 SCL       |
| CS     | 10        | OLED CS           |
| DC     | 9         | OLED D/C          |
| RST    | 8         | OLED RST          |
| MOSI   | 11        | OLED SDIN / MOSI  |
| SCK    | 13        | OLED SCLK / SCK   |
| MISO   | 12        | (not used)        |

I2C pull-ups: 4.7 kΩ from SDA to 3.3 V and SCL to 3.3 V (required).

---

## Diagnostic Tool

Flash the combined diagnostic:

```
pio run -e teensy41_combined_test -t upload
```

Open Serial Monitor at 115200. Interactive commands:

| Key | Action |
|-----|--------|
| `r` | Re-run pin census + I2C scan |
| `p` | Pin census only |
| `s` | SPI↔I2C short detect |
| `c` | 100-round PCF ping stability test |

If `c` shows 100% and setup shows 0% — pure power-on timing. Increase the `millis()` wait in `app.cpp`.

---

## Other Causes (Ruled Out for This Hardware)

These were considered and eliminated by the diagnostic:

| Cause | Evidence | Status |
|-------|----------|--------|
| Wiring short (SPI↔I2C) | Short detect: no shorts | Ruled out |
| Wrong OLED interface mode (I2C mode) | I2C scan: no unknown devices | Ruled out |
| 3.3 V rail voltage droop | Interleaved ping test: 100% stable | Ruled out |
| I2C bus locked (SDA stuck LOW) | Pin census: SDA HIGH at all times | Ruled out |
| Missing I2C pull-ups | SDA/SCL HIGH at step 1 | Pull-ups present |

If the symptom returns or changes, re-run the diagnostic and check which step fails first.
