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

## Phase 2 — Core MIDI pipeline ✓ COMPLETE

**Goal:** Get a note playing end-to-end: hardcoded pattern → tick → MIDI byte out. First proof-of-life.

- [x] Port `RingBufferSPSC<T,N>` to `src/core/ring_buffer.hpp`
- [x] Port `Timebase` / `TickEvent` / `Tempo` to `src/core/timebase.hpp`
- [x] Port `TickScheduler` to `src/core/tick_scheduler.hpp` (ISR → ring buffer)
- [x] Port `Transport` to `src/core/transport.hpp` (phase-accumulator, `onTick()`)
- [x] Rewrite `MidiIO` in `src/core/midi_io.hpp` — O(1) FIFO delay queue, no Serial in hot path
- [x] Rewrite `PlaybackEngine` in `src/engine/playback_engine.hpp` — cursor-based O(1) scan, `active_[64]`
- [x] Rewrite `RunLoop` in `src/core/runloop.hpp` — fixed buffer `MaxEvents=64`, `silenceAllTracks()` via CC
- [x] Wire into `src/app.cpp` `setup()`/`update()`

Exec plan: `exec-plans/active/02-core-pipeline.md`

---

## Phase 3 — Step Sequencer ✓ COMPLETE

**Goal:** Digitakt-style step editor: 16 keyboard buttons = 16 step slots, toggle notes on/off. Serial monitor for hardware-free testing.

- [x] Reduce to `MAX_TRACKS = 2` (saves ~80KB RAM1 vs 16 tracks)
- [x] Port `PCF8575` I2C driver to `src/io/pcf8575.hpp` (Serial behind `SEQUENCER_DEBUG`)
- [x] Port `IMatrixKBMode` interface to `src/io/matrix_kb_mode.hpp`
- [x] Implement `CursorMode` in `src/io/cursor_mode.hpp/.cpp` — page offset (enc 0), edit pitch (enc 1), ASCII step grid after every edit
- [x] Port `MatrixKB` to `src/io/matrix_kb.hpp/.cpp` — CTL 0 = toggle track, CTL 1 = play, CTL 2 = stop
- [x] Port `Encoder` + `EncoderManager` to `src/io/encoder.hpp/.hpp`
- [x] Write `SerialMonitor` in `src/io/serial_monitor.hpp` — `p`, `T<bpm>`, `G<steps>`, `A<tr>,<pitch>,<step>`, `X<tr>,<step>`, `C<tr>`, `L`, `?`
- [x] Wire `CursorMode`, `MatrixKB`, `EncoderManager`, `SerialMonitor` into `src/app.hpp/.cpp`

Exec plan: `exec-plans/active/03-step-sequencer.md`

---

## Phase 3.5 — Sequencer UX & Control Mapping ✓ COMPLETE

**Goal:** Finalize all control surface mappings before adding generators. Full CTL remap, encoder layers, settings mode, hold-step note editing.

- [x] CTL remap: `[REC][PLAY][STOP][PG+][MODE][TRK][SET][SHF]`
- [x] Settings mode (CTL 6): K1 → BPM ±0.5; press K1 = reset to 120
- [x] K5 encoder: step count ±1 (hold K5 + turn = ±16)
- [x] Hold-step editing: hold step button → K2=pitch, K3=velocity, K4=micro-offset
- [x] `AppEvent::ToggleSettings` + `RunLoop::consumeSettingsToggle()`
- [x] `CursorMode::editHeld()` + `getHeldStep()`
- [x] Control map reference doc: `docs/design-docs/control-map.md`

Exec plan: `exec-plans/active/03-step-sequencer.md` (extended)

---

## Phase 4 — Generator subsystem

**Goal:** Non-destructive algorithmic note generation, writes to `generative` layer.

- [ ] Rewrite `Generator` base in `src/engine/generator.hpp` — replace `std::map` params with `GeneratorParam params_[uint8_t(ParamId::_Count)]` fixed array
- [ ] Rewrite `GeneratorManager` in `src/engine/generator_manager.hpp` — replace `std::vector<std::unique_ptr>` with static array; add `NotePool<128> staging_`; two-phase `triggerGenerate()` + `applyPendingSwap()`
- [ ] Port `EuclideanGenerator` — replace `std::vector<bool> rhythm` with `bool rhythm[64]`; gate serial debug
- [ ] Hook `GeneratorManager::applyPendingSwap()` into `RunLoop` pre-tick-drain
- [ ] Verify: generator fills `generative` layer; recorded notes survive a generate call

Exec plan: `exec-plans/active/04-generator-redesign.md`

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
