# Matrix Keyboard Hardware Guide

## Overview

The sequencer uses a **3 × 8 button matrix** (24 buttons total) driven by a single **PCF8575** I2C 16-bit I/O expander.

- **8 column pins** — one per button column, read to detect presses  
- **3 row pins** — driven LOW one at a time to scan rows  
- **5 unused pins** — leave floating or tie HIGH

Scanning cycle: drive one row LOW → read 8 columns → any column reading LOW = button pressed in that row → restore row HIGH → repeat next row.

---

## Pin Assignment

| PCF8575 Pin | Role | Notes |
|-------------|------|-------|
| P00 | Column 0 | Step button 0 / 8 |
| P01 | Column 1 | Step button 1 / 9 |
| P02 | Column 2 | Step button 2 / 10 |
| P03 | Column 3 | Step button 3 / 11 |
| P04 | Column 4 | Step button 4 / 12 |
| P05 | Column 5 | Step button 5 / 13 |
| P06 | Column 6 | Step button 6 / 14 |
| P07 | Column 7 | Step button 7 / 15 |
| P08–P11 | Unused | Float or tie HIGH |
| P12 | Row 0 — CTL | REC, PLAY, STOP, PAGE, MODE, TRACK, SETTINGS, SHIFT |
| P13 | Row 1 — Top | Steps 0–7 |
| P14 | Row 2 — Bottom | Steps 8–15 |
| P15 | Unused | Float or tie HIGH |

---

## Correct Wiring

```
Teensy 4.1
  3V3 ──┬── 4.7kΩ ──┐
        │            │ (repeat ×8, one per column)
        │           P00 ─────────┬──── [button top] ──── [button bot] ────┐
        │           P01 ─────────┼──── ...                                 │
        │           ...          │                                          │
        │           P07 ─────────┘                                          │
        │                                                                   │
        │           P12 ────────────────────────────────────────── row CTL ─┘
        │           P13 ────────────────────────────────────────── row TOP ─┘
        │           P14 ────────────────────────────────────────── row BOT ─┘
        │
  GND ──┴── PCF8575 GND, A0=GND A1=GND A2=GND (address 0x20)

  SCL ── 4.7kΩ ── Teensy Pin 19
  SDA ── 4.7kΩ ── Teensy Pin 18
```

### In plain words

1. **Columns (P00–P07)**: one pin per column.  
   Each column pin connects to **one terminal** of every button in that column.  
   Each column pin also has a **10 kΩ pull-up resistor to 3.3V**.

2. **Rows (P12–P14)**: one pin per row.  
   Each row pin connects to **the other terminal** of every button in that row.

3. **Buttons**: a simple normally-open push-button between one row pin and one column pin.  
   No diodes required for a 3-row matrix.

4. **I2C pull-ups**: 4.7 kΩ from SDA and SCL to 3.3V (required if not already present on the Teensy).

---

## Why External Column Pull-ups Are Required

The PCF8575 has internal weak pull-ups (~100 µA source current, resulting in ~tens of kΩ equivalent). They are enough to sample an unloaded pin, but:

- If a row is driven LOW and a button is pressed, the row pin hard-drives the column pin to GND.  
- The PCF's internal pull-up (~100 µA) cannot fight a hard-driven LOW — the column stays LOW.
- Without an external resistor the column **floats** when no button is pressed, giving unreliable HIGH readings.

**Add 10 kΩ pull-ups to 3.3V on all 8 column pins (P00–P07).** Without them, columns may randomly read LOW and false-trigger.

---

## Scanning Algorithm (implemented in `matrix_kb.cpp`)

```
for each row r in {CTL, TOP, BOT}:
    write PCF: all HIGH except row r pin → LOW
    read  PCF: 16-bit value
    for each column c in 0..7:
        if bit cfg_.cols[c] is LOW → button pressed
write PCF: 0xFFFF   // restore all pins as inputs
```

Key detail: only **one row is driven LOW at a time**. Driving multiple rows simultaneously would cause ghost keys (two buttons in the same column but different rows would appear as a third "ghost" button).

---

## I2C Address Selection

| A2 | A1 | A0 | Address |
|----|----|----|---------|
| GND | GND | GND | 0x20 ✓ (default) |
| GND | GND | VCC | 0x21 |
| GND | VCC | GND | 0x22 |
| ... | ... | ... | ... |
| VCC | VCC | VCC | 0x27 |

All three address pins to GND → 0x20.

---

## Diagnosing a Non-Responsive PCF8575

### Step 1 — Check if the PCF is found on I2C

Expected serial output at boot:
```
[PCF8575] OK at 0x20
```

If you see `not found at 0x20`, the chip is not communicating. Check:
- VCC (2.5–5.5V, 3.3V recommended for Teensy)
- GND
- SDA / SCL wiring and pull-up resistors
- Address pin wiring

### Step 2 — PCF drive test (built into `matrix_kb.cpp` diagnostic)

```
[KB diag] write=0x0000 read=0xFFFF | write=0xFFFF read=0x0000
```

| write | expected read | what it means |
|-------|--------------|---------------|
| 0x0000 | ~0x0000 | PCF can drive pins LOW ✓ |
| 0x0000 | 0xFFFF | **PCF cannot drive LOW** — external pull-ups too strong, OR chip damaged |
| 0xFFFF | 0xFFFF | Pins float HIGH as expected ✓ |
| 0xFFFF | 0x0000 | **All pins shorted to GND** — wiring short, or chip damaged |

### Step 3 — Isolate chip vs wiring

**Disconnect all button matrix wires from the PCF** (leave only VCC, GND, SDA, SCL).

Run the drive test again:

- `write=0xFFFF read=0xFFFF` and `write=0x0000 read=0x0000` → **chip is fine**, fault is in matrix wiring (short to GND, or pull-ups missing/wrong value)
- Still `read=0xFFFF` when writing 0x0000 → **chip is damaged**, replace it

### Step 4 — Check for shorts in matrix wiring

With a multimeter in continuity mode:
1. Disconnect PCF power.
2. Measure each column pin (P00–P07) to GND — should be **open** (no continuity) when no button pressed.
3. Measure each row pin (P12–P14) to GND — should be **open** (no continuity).
4. Press a button — the column and row it connects **should** show continuity to each other, but **not** to GND.

If any column or row pin shows continuity to GND with no button pressed → you have a wiring short. Common causes:
- Solder bridge on PCB
- Wire accidentally touching GND rail
- Damaged PCF pin

### Step 5 — Verify pull-up resistors

Measure column pins (P00–P07) to 3.3V **with PCF powered and no buttons pressed**:
- Should read ~3.3V (pull-up working)
- If reads ~0V → missing or shorted pull-up

---

## Common Mistakes

| Mistake | Symptom | Fix |
|---------|---------|-----|
| No external column pull-ups | Columns float, random triggers | Add 10 kΩ from each column to 3.3V |
| Row and column pins swapped | Buttons never detected | Verify P12/P13/P14 are rows, P00–P07 are columns |
| All matrix wires shorted | All pins read LOW when released | Disconnect matrix, test PCF alone |
| PCF on wrong I2C address | `not found at 0x20` | Check A0–A2 pin wiring |
| No I2C pull-ups on SDA/SCL | I2C unreliable or fails | Add 4.7 kΩ to 3.3V on SDA and SCL |
| Driving multiple rows simultaneously | Ghost keys | Firmware already guards this — one row at a time |
| PCF5575 vs PCF8575 | Wrong bit count, wrong behaviour | Must be PCF**8**575 (16-bit), not PCF8574 (8-bit) |

---

## Component List (per keyboard unit)

| Component | Value | Qty |
|-----------|-------|-----|
| I2C I/O expander | PCF8575 (16-bit) | 1 |
| Pull-up, column | 10 kΩ, 0.1W | 8 |
| Pull-up, I2C | 4.7 kΩ, 0.1W | 2 (if not on Teensy breakout) |
| Push-button | 6×6mm SPST NO | 24 |
| Decoupling cap | 100 nF, 0402/0603 | 1 (across VCC–GND of PCF) |
