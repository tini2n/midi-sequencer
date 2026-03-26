# PLANS.md

Implementation roadmap. We go step by step — complete one phase before starting the next.

**Strategy:** Take module by module from `src/_legacy/`, review, refactor, fix, and reimplement into the new embedded-safe architecture. Start from the sequencer core and work outward. UI comes last.

**Priority order:** Data model → MIDI pipeline → Recording → Generators → UI

---

## Phase 1 — Data model ✓ COMPLETE

**Goal:** Replace heap-allocated model with fixed-size, embedded-safe types. Zero dependencies — start here.

- [x] Define `Note` in `src/model/note.hpp`
- [x] Define `NotePool<N>` in `src/model/note_pool.hpp` — fixed array, insertion sort, begin/end iterators
- [x] Define `LayeredTrack` in `src/model/track.hpp` — `NotePool<256> recorded` + `NotePool<128> generative`; 16 tracks in `Pattern`
- [x] Define `Pattern` in `src/model/pattern.hpp` — `LayeredTrack tracks[16]`, steps, grid, tempo
- [x] Define `Scale` in `src/model/scale.hpp` — 8 scales, bitmask lookup, heap-free
- [x] Define `Viewport` in `src/ui/viewport.hpp` — pan/zoom, moved to `ui/` (not model)

Exec plan: `exec-plans/active/01-data-model.md`

---

## Phase 2 — Core MIDI pipeline (current)

**Goal:** Get a note playing end-to-end: hardcoded pattern → tick → MIDI byte out. First proof-of-life.

- [ ] Port `RingBufferSPSC<T,N>` to `src/core/ring_buffer.hpp` (legacy version is correct; verify, port)
- [ ] Port `Timebase` / `TickEvent` / `Tempo` to `src/core/timebase.hpp`
- [ ] Port `TickScheduler` to `src/core/tick_scheduler.hpp` (ISR → ring buffer; already correct design)
- [ ] Port `Transport` to `src/core/transport.hpp` (clean state machine; port with minor fixes)
- [ ] Rewrite `MidiIO` in `src/core/midi_io.hpp` — fix O(n) queue pop → `RingBufferSPSC<Q,32>`; gate all `Serial.printf` behind `SEQUENCER_DEBUG`
- [ ] Rewrite `PlaybackEngine` in `src/engine/playback_engine.hpp` — cursor-based O(1) tick scan, iterates both `LayeredTrack` pools
- [ ] Rewrite `RunLoop` in `src/core/runloop.hpp` — remove `std::vector<MidiEvent>`, replace with fixed-size buffer; wire tick drain → PlaybackEngine → MidiIO
- [ ] Wire into `src/app.cpp` `setup()`/`update()` — hardcode a test pattern, verify MIDI output

Exec plan: `exec-plans/active/02-core-pipeline.md` *(to be written)*

---

## Phase 3 — Recording

**Goal:** Input notes from hardware, store in pattern, play back. Closes the record → playback loop.

- [ ] Rewrite `RecordEngine` in `src/engine/record_engine.hpp` — replace `std::unordered_map` with `Pending pending_[128]` + `bool active_[128]`; writes to `track.recorded`
- [ ] Port `PCF8575` I2C driver to `src/io/pcf8575.hpp`
- [ ] Port `MatrixKB` to `src/io/matrix_kb.hpp` — gate all `Serial.printf` behind `SEQUENCER_DEBUG`; connect to `RecordEngine`
- [ ] Port `Encoder` to `src/io/encoder.hpp` (already heap-free; minor cleanup)
- [ ] Port `EncoderManager` to `src/io/encoder_manager.hpp`
- [ ] Verify: play notes on keyboard → notes stored → play back via MIDI out

---

## Phase 4 — Generator subsystem

**Goal:** Non-destructive algorithmic note generation, writes to `generative` layer.

- [ ] Rewrite `Generator` base in `src/engine/generator.hpp` — replace `std::map` params with `GeneratorParam params_[uint8_t(ParamId::_Count)]` fixed array
- [ ] Rewrite `GeneratorManager` in `src/engine/generator_manager.hpp` — replace `std::vector<std::unique_ptr>` with static array; add `NotePool<128> staging_`; two-phase `triggerGenerate()` + `applyPendingSwap()`
- [ ] Port `EuclideanGenerator` — replace `std::vector<bool> rhythm` with `bool rhythm[64]`; gate serial debug
- [ ] Hook `GeneratorManager::applyPendingSwap()` into `RunLoop` pre-tick-drain
- [ ] Verify: generator fills `generative` layer; recorded notes survive a generate call

Exec plan: `exec-plans/active/03-generator-redesign.md` *(renumbered from 02)*

---

## Phase 5 — UI

**Goal:** Display and encoder-driven control. Sequencer must be fully working before this phase.

- [ ] Port `OledRenderer` + `PianoRoll` widget — thin U8G2 wrapper; no domain logic changes
- [ ] Wire `Viewport` (`src/ui/viewport.hpp` — already created) into pan/zoom encoder controls
- [ ] Define `IView` interface in `src/ui/views/base_view.hpp`
- [ ] Rewrite `PerformanceView` — remove duplicate MatrixKB logic; reuse `MatrixKB` + `IMatrixKBMode`
- [ ] Port `GenerativeView` — wire to `GeneratorManager` new param API
- [ ] Port `ViewManager`

---

## Phase 6 — Scale & generative modes

**Goal:** Expand musical vocabulary.

- [x] Scales already in `src/model/scale.hpp` (Major, Minor, Dorian, Phrygian, Lydian, PentaMajor, PentaMinor, None)
- [ ] Add Chromatic scale if needed
- [ ] Add Markov chain generator
- [ ] Add probability-grid generator
- [ ] Add cellular automata generator
- [ ] Seed/determinism controls per generator

---

## Backlog

- Multi-pattern chaining (song mode)
- Per-track MIDI channel routing
- Clock in (external sync)
