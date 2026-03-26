# CLAUDE.md

This file provides guidance to Claude Code (claude.ai/code) when working with code in this repository.

## What This Is

A polyphonic MIDI sequencer firmware for Teensy 4.1 (C++17, PlatformIO). It records and plays back MIDI on up to 16 tracks, generates rhythmic patterns algorithmically (Euclidean, future: Markov), and drives a 256×64 SSD1322 OLED display with a piano-roll UI. The project is mid-refactor — see [PLANS.md](docs/PLANS.md) and the exec plans in `docs/exec-plans/active/` before starting any work.

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

**Current state:** Phase 1 complete — `src/model/` and `src/ui/viewport.hpp` are done. All other layers (`core/`, `engine/`, `io/`, remaining `ui/`) are pending — reference implementations live in `src/_legacy/`.

### Tick System

The entire timing chain flows from a 1 kHz `IntervalTimer` ISR:

```
IntervalTimer ISR (1 kHz)
  → pushes TickEvent into RingBuffer<1024> (SPSC, ISR-safe)
RunLoop::service() (called every Arduino loop())
  → drains ring buffer → Transport::on1ms()
  → Transport advances playhead (96 PPQN)
  → PlaybackEngine::processTick(prev, curr, pattern) → MidiEvents
  → MidiIO::send() → serial out
```

Key timing: PPQN=96, one 1/16 step = 24 ticks, one 4/4 bar = 384 ticks. Always use `timebase::ticksPerStep()` from `src/types.hpp` — never hardcode tick math.

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
| `src/app.hpp/cpp` | Top-level `App::setup()` / `App::update()` (currently stubs) |
| `src/model/` | Complete new model layer (note, note_pool, track, pattern, scale) |
| `src/ui/viewport.hpp` | Display window state (time pan/zoom + pitch pan) — Phase 1 complete |
| `src/_legacy/` | Reference implementations to port from |
| `docs/ARCHITECTURE.md` | System diagram and subsystem descriptions |
| `docs/DESIGN.md` | Design rules and rationale |
| `docs/design-docs/` | Deep dives: memory model, generative model, core beliefs |
| `docs/exec-plans/active/` | Step-by-step implementation plans per phase |
| `docs/exec-plans/tech-debt-tracker.md` | Known issues with severity ratings |
