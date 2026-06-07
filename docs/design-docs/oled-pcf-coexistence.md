# OLED (SSD1322) + PCF8575 Coexistence — Diagnosis & Fixes

Hardware: Teensy 4.1, SSD1322 NHD 256×64 OLED (SPI), PCF8575 I2C 16-bit I/O expander (matrix keyboard).

---

## Current Diagnosis

**Confirmed:**
- No SPI↔I2C wiring shorts.
- No wrong OLED interface mode (no extra I2C devices).
- All pins read HIGH before init — I2C pull-ups are present.

**Confirmed failure mode:**
- OLED connected + powered → PCF8575 completely absent from I2C bus. Not found even after 10 seconds of continuous polling.
- OLED disconnected → PCF works normally.

**Root cause: 3.3V rail voltage collapse.**

The OLED module (SSD1322 + panel + boost converter) draws significant current from the 3.3V rail when powered. The Teensy 4.1's onboard 3.3V regulator cannot supply enough current for both devices simultaneously without the rail drooping. The PCF8575 minimum operating voltage is 2.5V; below this it stops responding entirely and disappears from the bus. The Teensy itself survives because it's decoupled closer to the regulator, but the PCF (further down the power rail, with no local bulk capacitance) sees a lower voltage.

The `digitalRead(SDA)=HIGH` in the pin census is not proof the rail is healthy — the Teensy's input threshold is lower than PCF8575's minimum VCC, so the pin reads HIGH even when the rail is too low for the PCF to operate.

---

## Fix Priority Order

### Fix 1 — Power OLED from 5V (recommended, permanent)

Most NHD SSD1322 modules have an onboard linear regulator and accept 5V on their VCC/VIN pin. The logic signal pins (CS, DC, RST, MOSI, SCK) stay at 3.3V Teensy levels.

- Connect OLED VCC/VIN to Teensy `Vin` pin (5V from USB).
- Keep all signal wires on 3.3V Teensy pins as-is.
- PCF8575 VCC stays on 3.3V.

This removes the OLED from the 3.3V rail entirely. The Teensy 4.1's `Vin` pin is directly from USB (or external supply) with no 3.3V regulator in the path.

> The user previously noted the screen became brighter when moved to 5V — this confirms the module accepts 5V input and has a separate internal regulator. If PCF still fails on 5V, the module's logic circuitry has a separate VDDIO pin that must also be checked — it may still be pulling from 3.3V.

### Fix 2 — Add bulk capacitance near PCF8575 (hardware, always do this)

Regardless of which power fix you choose, add decoupling at the PCF8575:

- **100 µF electrolytic** between VCC (pin 24) and GND (pin 12) — as close to the chip as possible.
- **100 nF ceramic** in parallel with the electrolytic.

This prevents transient droops from resetting the PCF during OLED frame bursts.

### Fix 3 — Add bulk capacitance on the 3.3V rail

- **470 µF electrolytic** + **100 nF ceramic** near the Teensy 3.3V output pin.

---

## How to Verify the Fix

After applying Fix 1 (OLED on 5V):

1. Flash the combined diagnostic.
2. Run Step 7 (timed poll). PCF should respond within 1–2 seconds.
3. Run `c` (100-ping stability test). Should show 100% OK.
4. Flash main firmware. Keyboard should work.

If PCF still absent after Fix 1: the OLED module's logic VDDIO is still on 3.3V — check whether the module has a separate logic supply pin.

---

## What Was Ruled Out

| Cause | Evidence |
|-------|----------|
| Wiring short (SPI↔I2C) | Short detect: clean |
| Wrong OLED interface mode | I2C scan: no extra devices |
| I2C bus locked (SDA stuck LOW) | Pin census: SDA HIGH at all times |
| Missing I2C pull-ups | SDA/SCL HIGH before init |
| Timing race (PCF slow to boot) | 10-second timed poll: no response at all |

---

## SSD1322 Interface Mode Reference

Interface selected via BS0, BS1, BS2 pins (set by SMD resistors R5/R8 on NHD modules).

| BS2 | BS1 | BS0 | Interface      | Notes                                          |
|-----|-----|-----|----------------|------------------------------------------------|
|  1  |  0  |  0  | **4-wire SPI** | CS, SCLK, SDIN, D/C — **current mode (correct)** |
|  1  |  0  |  1  | 3-wire SPI     | D/C embedded in data stream                    |
|  0  |  1  |  1  | 68xx parallel  | 6800-series 8-bit parallel bus                 |
|  0  |  1  |  0  | 80xx parallel  | 8080-series 8-bit parallel bus                 |

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

I2C pull-ups: 4.7 kΩ from SDA to 3.3V, SCL to 3.3V.

---

## Diagnostic Tool

```
pio run -e teensy41_combined_test -t upload
```

| Key | Action |
|-----|--------|
| `r` | Pin census + I2C scan |
| `p` | Pin census only |
| `s` | SPI↔I2C short detect |
| `c` | 100-round PCF stability ping |
| `t` | Timed startup poll (up to 10s) |
