# Exec Plan 01 — Data Model Redesign

**Status:** ✓ COMPLETE
**Goal:** Replace heap-allocated model with fixed-size, embedded-safe types.
**Touches:** `src/model/`, `src/ui/viewport.hpp`

---

## Steps

### 1. ✓ Define `Note` in `src/model/note.hpp`

### 2. ✓ Define `NotePool<N>` in `src/model/note_pool.hpp`

### 3. ✓ Define `LayeredTrack` in `src/model/track.hpp`

`NotePool<256> recorded` + `NotePool<128> generative` + `uint8_t channel`.

### 4. ✓ Define `Pattern` in `src/model/pattern.hpp`

`LayeredTrack tracks[16]` (16-track Digitakt-style). Each track has an optional `steps` override for polyrhythm (TODO, reserved field). `ticks()` = `ticksPerStep(grid) * steps`.

### 5. ✓ Define `Scale` in `src/model/scale.hpp`

8 scales (None, Major, Minor, Dorian, Phrygian, Lydian, PentaMajor, PentaMinor). 12-bit bitmask per scale for O(1) `contains()`. Correct `degreeCount()` for pentatonic (5, not 7).

### 6. ✓ Define `Viewport` in `src/ui/viewport.hpp`

Moved to `src/ui/` (not model — UI-only concept). Removed legacy `grid` field (belongs on Pattern). Added `pixelsPerTick()` helper. No heap, no model dependency.

---

## Result

All files created. No `std::vector`, `std::map`, or `new` anywhere in `src/model/` or `src/ui/viewport.hpp`. Ready for Phase 2.
