# CLAUDE.md

This file provides guidance to Claude Code (claude.ai/code) when working with code in this repository.

## What This Is

A polyphonic MIDI step sequencer firmware for Teensy 4.1 (C++17, PlatformIO). Digitakt-style: 16 keyboard buttons = 16 step slots, toggle notes on/off per track. 2 active tracks, each on its own MIDI output channel. Plays back via MIDI out with no heap allocation in the hot path. The project is mid-refactor — see [PLANS.md](docs/PLANS.md) and the exec plans in `docs/exec-plans/active/` before starting any work.

## Build & Upload

```bash
pio run -e teensy41              # Build main firmware
pio run -e teensy41 -t upload    # Upload to Teensy 4.1
pio device monitor               # Serial monitor (115200 baud)
```

There is no unit test suite. Tests are hardware-in-the-loop environments on the Teensy:

```bash
pio run -e teensy41_encoder_test
pio run -e teensy41_cursor_test
```

## Architecture

The codebase is organized into layers with strict dependency rules (lower layers cannot depend on higher):

```
App (src/app.hpp)
 ├── Core     — TickScheduler (1 kHz ISR), Transport, RunLoop, MidiIO
 ├── Engine   — PlaybackEngine, RecordEngine, GeneratorManager
 ├── IO       — MatrixKB (PCF8575 I2C keyboard), EncoderManager
 ├── UI       — ViewManager, OledRenderer (U8g2)
 └── Model    — Pattern, LayeredTrack, NotePool<N>, Note, Scale
```

**Current state:** Phases 1–3 complete. Full step sequencer is working: model, MIDI pipeline, playback engine, keyboard, encoders, and serial monitor are all wired. `src/_legacy/` is excluded from the build (`build_src_filter` in `platformio.ini`). Generators (Phase 4) and OLED UI (Phase 5) are next.

### Tick System

The entire timing chain flows from a 1 kHz `IntervalTimer` ISR:

```
IntervalTimer ISR (1 kHz)
  → pushes TickEvent into RingBufferSPSC<1024> (ISR-safe)
RunLoop::service() (called every Arduino loop())
  → drains ring buffer → Transport::onTick()
  → Transport advances playhead (96 PPQN phase accumulator)
  → PlaybackEngine::processTick(prev, curr, pattern) — O(1) cursor scan
  → MidiIO::send() → Serial1 (hardware MIDI out)
```

Key timing: PPQN=96, one 1/16 step = 24 ticks, one 4/4 bar = 384 ticks. Always use `timebase::ticksPerStep()` from `src/types.hpp` — never hardcode tick math.

### Step Sequencer & Control Surface (Phases 3–3.5 — complete)

`CursorMode` implements Digitakt-style step editing:
- 16 keyboard buttons map to 16 step slots. Press = toggle note on/off at that step.
- `pageOffset_` (K1/Enc 0): actual step = btn + pageOffset × 16. Lets you edit beyond 16 steps.
- `editPitch_` (K2/Enc 1): MIDI note assigned to new trigs.
- `trackIdx_` (CTL 5): toggle active track 0↔1.
- After every note change, `printTrackState()` prints an ASCII step grid to Serial.

**CTL buttons:** `[REC][PLAY][STOP][PG+][MODE][TRK][SET][SHF]` (indices 0–7)

**Hold-step editing:** Hold a step button and turn K2/K3/K4 to edit pitch/velocity/micro-offset of that note live. `CursorMode::editHeld(param, delta, pat)` — called from `App::onEncoderRotation()` when `cursor_.getHeldStep() >= 0`.

**Settings mode (CTL 6):** K1 controls BPM (±0.5 per detent; press = reset to 120). Toggle via `RunLoop::consumeSettingsToggle()` checked in `App::update()`.

**K5 (Enc 4):** step count ±1 per detent; hold K5 button while turning = ±16 steps.

Full control map: `docs/design-docs/control-map.md`.

`SerialMonitor` enables hardware-free testing: `A0,60,0` adds a note, `L` shows the grid, `p` plays.

### Note Layers

Each `LayeredTrack` has two independent `NotePool` layers:
- **recorded**: notes the user played in (max 256)
- **generative**: algorithmically generated notes (max 128)

Generators write to a staging buffer and swap atomically — they never overwrite recorded notes. Both layers are scanned together during playback.

## Critical Constraints

**No heap after `setup()`** — no `new`, no `std::vector`, no `std::map`, no `std::string` in new code. Use fixed-size arrays or `NotePool<N>`. This is a hard rule: heap fragmentation on embedded crashes the device.

**ISR is sacred** — the 1 kHz `IntervalTimer` ISR only pushes to the ring buffer. No I2C, no SPI, no Serial, no allocation in the ISR.

**Generators are non-destructive** — always write to a staging buffer; swap into `track.generative` atomically from the run loop, never from within a generator call.

**One source of truth for tick math** — `timebase::ticksPerStep()` in `src/types.hpp`.

**Playback is cursor-based, O(1) per tick** — `PlaybackEngine` tracks a cursor into the sorted `NotePool` and advances it; it does not scan all notes every tick.

## Working Rules

1. Read `docs/exec-plans/active/` before starting any new subsystem — each phase has a detailed plan.
2. When porting from `src/_legacy/`, replace all heap containers (`std::vector`, `std::map`) with fixed-size alternatives.
3. Gate debug output with `#ifdef SEQUENCER_DEBUG`.
4. If you find a code quality issue that can't be fixed now, add it to `docs/exec-plans/tech-debt-tracker.md`.

## Key Files

| File | Purpose |
|------|---------|
| `src/types.hpp` | `TickEvent`, `PPQN`, `timebase::ticksPerStep()` — timing primitives |
| `src/config.hpp` | Hardware pin assignments, I2C addresses, tick rate |
| `src/app.hpp/.cpp` | Top-level `App::setup()` / `App::update()`; implements `IEncoderHandler` |
| `src/model/` | Model layer: `note.hpp`, `note_pool.hpp`, `track.hpp`, `pattern.hpp`, `scale.hpp` |
| `src/core/` | `ring_buffer.hpp`, `timebase.hpp`, `tick_scheduler.hpp`, `transport.hpp`, `midi_io.hpp`, `runloop.hpp` |
| `src/engine/playback_engine.hpp` | Cursor-based O(1) note scanning; `active_[64]` for note-off tracking |
| `src/io/pcf8575.hpp` | PCF8575 I2C driver (all debug behind `SEQUENCER_DEBUG`) |
| `src/io/matrix_kb_mode.hpp` | `IMatrixKBMode` interface — implement to add a new keyboard mode |
| `src/io/cursor_mode.hpp/.cpp` | Digitakt-style step editor — the primary UI mode |
| `src/io/matrix_kb.hpp/.cpp` | PCF8575-backed keyboard driver |
| `src/io/encoder.hpp` + `encoder_manager.hpp` | Quadrature encoders (8 managed, dispatches via `IEncoderHandler`) |
| `src/io/serial_monitor.hpp` | USB serial command interface for hardware-free testing |
| `src/ui/viewport.hpp` | Display window state (time pan/zoom + pitch pan) — used in Phase 5 |
| `src/_legacy/` | Reference implementations (excluded from build via `build_src_filter`) |
| `docs/exec-plans/active/` | Step-by-step implementation plans per phase |
| `docs/exec-plans/tech-debt-tracker.md` | Known issues with severity ratings |
