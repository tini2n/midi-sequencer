# Encoder Hardware Guide

## Overview

Eight **EC11-type** rotary encoders with integrated push switch. Each encoder has:
- **A / B** — quadrature output channels (normally-open contacts, common to GND)
- **SW** — push switch (normally-open, common to GND)

All pins use `INPUT_PULLUP`. At mechanical rest, both A and B are open → pulled HIGH → `readAB() = 0b11`.

---

## Pin Assignments

| Encoder | Teensy pinA | Teensy pinB | Teensy pinSW | A/B swapped? | Known issue |
|---------|-------------|-------------|--------------|--------------|-------------|
| K1 | 2 | 3 | 0 | no | — |
| K2 | 4 | 5 | 12 | no | — |
| K3 | 6 | 7 | 26 | **yes** | reversed in software |
| K4 | 14 | 15 | 27 | **yes** | reversed in software; SW not triggering (pin 27 miswired or floating) |
| K5 | 16 | 17 | 28 | no | — |
| K6 | 20 | 21 | 29 | **yes** | reversed in software |
| K7 | 22 | 23 | 30 | **yes** | reversed in software; left rotation cross-triggers K8 SW — hardware wiring bug, cannot fix in software |
| K8 | 24 | 25 | 31 | **yes** | reversed in software |

---

## Direction Normalization

Some encoders are mounted or wired with A and B swapped relative to the others.
The convention is: **left turn → −1, right turn → +1**.

Rather than rewiring, set `reversed = true` in `kEncoderPins` (`src/app.cpp`):

```cpp
struct PinConfig { uint8_t pinA, pinB, pinSW; bool reversed{false}; };
```

When `reversed = true`, `Encoder::poll()` negates the result before returning, so all callers see a consistent sign convention regardless of physical wiring polarity.

---

## Quadrature Decoding

`Encoder::poll()` uses a 4×4 state-transition table (Gray code):

```
readAB() encodes: bit1 = digitalRead(pinB), bit0 = digitalRead(pinA)
Rest state (kDetent) = 0b11 (both channels open = HIGH via pullup)

One CW detent cycle (typical): 0b11 → 0b10 → 0b00 → 0b01 → 0b11
One CCW detent cycle:           0b11 → 0b01 → 0b00 → 0b10 → 0b11
```

Transitions accumulate in `raw_`. A value is emitted **only** when the shaft snaps back to `kDetent` (0b11) and `raw_ != 0`. This eliminates pre-click mechanical noise — partial rotations that return to rest discard their accumulated raw_ cleanly.

### Why kTable is function-local static

`kTable` lives as `static constexpr` inside `poll()` rather than as a class-level static member. Class-level `static constexpr` arrays can be ODR-used on ARM GCC (even with -std=gnu++17) if the compiler decides to take the address of an element, causing the linker to look for an out-of-class definition that doesn't exist and silently link zeros. Function-local `static constexpr` always ends up in `.rodata` with no ODR ambiguity.

---

## Sensitivity Troubleshooting

**Symptom: need multiple detents to register a value change**

1. **Check poll rate** — `enc_.poll()` is called from `App::update()`. Any blocking operation in `loop_.service()` or `serial_.poll()` delays the next poll. Missed quadrature transitions mean `raw_` doesn't accumulate, so `kDetent` fires with `raw_ = 0` and emits nothing. Monitor with `[ENC]` debug output and count missed detents.

2. **Check rest state** — Verify the encoder actually rests at `readAB() = 0b11`. Open the serial monitor and watch `[ENC]` output on a very slow single-detent turn. If it emits at unexpected counts, the physical rest state may differ from `kDetent`. Some encoders rest at 0b00 — in that case, change `kDetent` to match.

3. **Interrupt-driven fallback** — If polling is fundamentally too slow, switch to `attachInterrupt` on pinA for each encoder. Accumulate `raw_` in the ISR and only read + reset it in `poll()`. This guarantees no transitions are missed at any poll rate.

---

## Known Hardware Bugs

### K4 SW (pin 27) — not triggering

The push switch press/release for K4 is not detected. The SW pin (27) is likely miswired or floating. Until fixed in hardware, K4 press/reset (edit length → 1) is unavailable. All rotation functions for K4 work normally.

### K7 left rotation cross-triggers K8 SW

Turning K7 left fires `[ENC] K8 press` and `[ENC] K8 release` events. This means one of K7's encoder pins (22 or 23) is sharing a PCB trace with K8's SW pin (31). Cannot be corrected in software. Impact: K7 left turns also fire K8 SW handler (currently a no-op for K8 since K8 is reserved). When K8 is assigned a function, this cross-trigger must be resolved in hardware first.
