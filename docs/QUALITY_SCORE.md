# QUALITY_SCORE.md

Tracked quality metrics for the codebase. Update after each phase.

## Current State (legacy, pre-redesign)

| Area | Score | Notes |
|------|-------|-------|
| Memory safety | 2/10 | std::vector/map in hot paths; heap fragmentation on long sessions |
| Performance | 3/10 | O(n) tick scan; O(n) queue pop; blocking Serial in hot paths |
| Generator correctness | 4/10 | Euclidean algo is correct; param map uses pointer keys (UB) |
| Separation of concerns | 5/10 | Views mostly clean; generators write directly to pattern (bad) |
| Embedded fitness | 3/10 | std containers, allNotesOff loops, debug always-on |

## Target (post Phase 3)

| Area | Target | How |
|------|--------|-----|
| Memory safety | 9/10 | Fixed-size pools, no heap in hot paths |
| Performance | 9/10 | O(1) tick scan via cursor, ring buffer queue |
| Generator correctness | 9/10 | Fixed param array, non-destructive generate |
| Separation of concerns | 9/10 | Generators write to staging, layers in model |
| Embedded fitness | 9/10 | No dynamic alloc, debug gated by compile flag |

## What to Check Before Each PR

- No `std::vector`, `std::map`, `std::string`, `new`, or `malloc` in `core/`, `engine/`, or `model/`
- No `Serial.printf` outside `#ifdef SEQUENCER_DEBUG` in `engine/` or `core/`
- `playback_engine` processes tick in O(1) (cursor advance, not full scan)
- Generator `generate()` takes a `NotePool&` output param, does not touch `Pattern` directly
