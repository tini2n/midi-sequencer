# AGENTS.md — AI Agent Guide

This file tells AI agents how to work in this codebase.

## Project

Embedded MIDI sequencer on **Teensy 4.1** (Arduino/PlatformIO, C++17).
Hardware: SSD1322 256×64 OLED (SPI), PCF8575 matrix keyboard (I2C), 8 rotary encoders.

## Critical Constraints

- **No heap allocation in hot paths.** The main loop runs at ~30 FPS; the tick ISR fires at 1 kHz.
  `std::vector`, `std::map`, `std::string`, and `new`/`delete` in hot paths cause heap fragmentation
  and eventual crash on long sessions. Use fixed-size arrays or static pools.
- **No blocking I/O in ISR or RunLoop.** `Serial.printf` in `GeneratorManager` and
  `PlaybackEngine` hot paths must be removed. Serial output belongs in debug-only compile paths.
- **Single-core, cooperative multitasking.** There is no RTOS. ISR → ring buffer → main loop.
  Keep ISRs minimal.

## Repository Layout

```
src/
  _legacy/          # Old code being replaced — read for reference, don't extend
  config.hpp        # Hardware constants (tick rate, I2C address, etc.)
  types.hpp         # TickEvent, Tempo, PPQN
  main.cpp          # Arduino setup()/loop() — currently empty stub
test/
  legacy_main.cpp   # Old wiring of all subsystems — read to understand intent
docs/               # Design docs, plans, references (see docs/PLANS.md)
ARCHITECTURE.md     # System overview
```

## Working Rules

1. **Read before touching.** Always read the file before editing.
2. **No std::vector/map in new code unless bounded and pre-allocated.**
   Prefer `std::array`, fixed-size ring buffers, or static pools.
3. **Generators must not modify Pattern directly** during generation.
   Write to a staging buffer; swap atomically.
4. **One source of truth for tick math.** Use `timebase::ticksPerStep()` from `types.hpp`.
   Do not recompute `(96 * 4) / grid` inline elsewhere.
5. **Follow the exec-plan.** Check `docs/exec-plans/active/` before starting new work.
6. **Mark tech debt.** When you notice a problem but can't fix it now, add it to
   `docs/exec-plans/tech-debt-tracker.md`.

## Build

```bash
# Main firmware
pio run -e teensy41

# Upload
pio run -e teensy41 -t upload

# Serial monitor
pio device monitor
```
