# DESIGN.md

Core design decisions for the new implementation.
See `design-docs/` for reasoning behind each decision.

## Rules

1. **No heap in hot paths.** All note storage, event queues, and generator output use fixed-size
   static or stack-allocated containers.

2. **Generators are non-destructive.** A generator writes to a `NotePool` staging buffer.
   The buffer is swapped into the active pattern only when generation is complete.

3. **Layers, not a flat track.** A `Pattern` contains:
   - a **recorded layer** (user-played notes, survives generate)
   - a **generative layer** (output of last generator run, replaced on next generate)
   Both layers are mixed by `PlaybackEngine` at playback time.

4. **Sorted notes, windowed scan.** Notes are kept sorted by `on` tick.
   `PlaybackEngine` uses a cursor index advanced each tick — O(1) per tick instead of O(n).

5. **Parameters as fixed arrays, not maps.** Generator parameters are stored in a fixed
   `GeneratorParam params[MAX_PARAMS]` array indexed by enum. No heap, no pointer-key bugs.

6. **Debug output is compile-gated.** `#ifdef SEQUENCER_DEBUG` wraps all `Serial.printf`.
   Hot paths have zero serial output in release builds.

7. **Tick math in one place.** `timebase::ticksPerStep()` in `types.hpp` is the single
   source of truth. No inline `(96*4)/grid` scattered through the code.
