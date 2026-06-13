# CLAUDE.md

This file provides guidance to Claude Code (claude.ai/code) when working with code in this repository.

## Efficency Core

- **Brevity-First:** Prioritize high information density. Eliminate filler phrases, conversational "fluff," and social pleasantries (e.g., "Certainly," "I hope this helps").
- **Zero Redundancy:** Do not restate the user’s prompt or explain what you are about to do. Start the response immediately at the first point of value.
- **Structural Efficiency:** Use Markdown (bullets, tables, headers) to convey relationships between ideas. Avoid long-form prose unless narrative flow is essential to the task.
- **Implicit Context:** Assume a high level of competence. Do not explain basic concepts unless explicitly requested or necessary for the solution.
- **Omit Meta-Talk:** No self-references ("As an AI," "Based on my training"). 
- **Direct Resolution:** Address the core intent of the query with the fewest tokens possible while maintaining technical/creative integrity.

## What This Is

A polyphonic MIDI step sequencer firmware for Teensy 4.1 (C++17, PlatformIO). Digitakt-style: 16 keyboard buttons = 16 step slots, toggle notes on/off per track. 2 active tracks, each on its own MIDI output channel. Plays back via MIDI out with no heap allocation in the hot path. The project is mid-refactor — see [PLANS.md](docs/PLANS.md) and the exec plans in `docs/exec-plans/active/` before starting any work.

## Critical Constraints

**No heap after `setup()`** — no `new`, no `std::vector`, no `std::map`, no `std::string` in new code. Use fixed-size arrays or `NotePool<N>`. This is a hard rule: heap fragmentation on embedded crashes the device.

**ISR is sacred** — the 1 kHz `IntervalTimer` ISR only pushes to the ring buffer. No I2C, no SPI, no Serial, no allocation in the ISR.

**Generators are non-destructive** — always write to a staging buffer; swap into `track.generative` atomically from the run loop, never from within a generator call.

**One source of truth for tick math** — `timebase::ticksPerStep()` in `src/types.hpp`.

**Playback is cursor-based, O(1) per tick** — `PlaybackEngine` tracks a cursor into the sorted `NotePool` and advances it; it does not scan all notes every tick.

**Layers, not a flat track.** A `Pattern` contains:
- a **recorded layer** (user-played notes, survives generate)
- a **generative layer** (output of last generator run, replaced on next generate)
Both layers are mixed by `PlaybackEngine` at playback time.

**Sorted notes, windowed scan.** Notes are kept sorted by `on` tick.
`PlaybackEngine` uses a cursor index advanced each tick — O(1) per tick instead of O(n).

**Parameters as fixed arrays, not maps.** Generator parameters are stored in a fixed
`GeneratorParam params[MAX_PARAMS]` array indexed by enum. No heap, no pointer-key bugs.

**Debug output is compile-gated.** `#ifdef SEQUENCER_DEBUG` wraps all `Serial.printf`.
Hot paths have zero serial output in release builds.

**Tick math in one place.** `timebase::ticksPerStep()` in `types.hpp` is the single
source of truth. No inline `(96*4)/grid` scattered through the code.