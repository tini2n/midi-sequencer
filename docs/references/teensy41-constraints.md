# Teensy 4.1 — Hardware Constraints Reference

## Memory

| Region | Size | Notes |
|--------|------|-------|
| Flash (ITCM) | 512 KB fast | Code executed from here |
| Flash (OCRAM) | 7.75 MB | Large constants, lookup tables |
| RAM1 (DTCM) | 512 KB | Stack + most heap — fast |
| RAM2 (OCRAM2) | 512 KB | `DMAMEM` attribute — slower |
| External PSRAM (optional) | up to 16 MB | `EXTMEM` attribute — slow |

**Rule:** All note pools and engine state go in RAM1. No `EXTMEM` for hot data.

## CPU

- ARM Cortex-M7 @ 600 MHz
- FPU (float is cheap, double is slower)
- No MMU → no segfault protection → a bad pointer silently corrupts memory

## Timers

- `IntervalTimer` uses hardware PITs — resolution 1 µs, up to 4 simultaneous.
- We use 1 timer for the tick scheduler (1 kHz = 1000 µs interval).

## I2C

- `Wire` (I2C0) used for PCF8575 matrix keyboard.
- `Wire.setClock(400000)` — 400 kHz fast-mode.
- PCF8575 round-trip (write + read) takes ~100 µs at 400 kHz — do not call from ISR.

## SPI

- Hardware SPI used for SSD1322 OLED.
- SPI clock up to 30 MHz for SSD1322 — U8g2 handles timing.

## MIDI

- `Serial1` at 31250 baud — standard MIDI.
- Byte transmit time = 320 µs (10 bits / 31250). A 3-byte Note On = ~960 µs.
- `allNotesOff` (128 × 3 bytes) = ~368 ms — only use as panic, not regularly.

## PlatformIO Build

```ini
[env:teensy41]
platform = teensy
board = teensy41
framework = arduino
build_flags = -std=gnu++17
```

`std::array`, `std::atomic`, `<algorithm>` are available.
`std::vector`, `std::map`, `std::string` compile but use heap — avoid.
