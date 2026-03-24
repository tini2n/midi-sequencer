# PLANS.md

Implementation roadmap. We go step by step — complete one phase before starting the next.

## Phase 1 — Data model (current)

Goal: replace heap-allocated model with fixed-size, embedded-safe equivalents.

- [ ] Define `NotePool<N>` — fixed array of `Note` with a size counter
- [ ] Replace `Track::notes` (`std::vector`) with `NotePool`
- [ ] Define `LayeredTrack` — recorded pool + generative pool
- [ ] Replace `Pattern::track` with `LayeredTrack`
- [ ] Update `PlaybackEngine` to iterate both layers
- [ ] Add sorted-insert to `NotePool`, add cursor-based `processTick`

Exec plan: `exec-plans/active/01-data-model.md`

## Phase 2 — Generator subsystem

Goal: fix generator memory, parameter system, and non-destructive generation.

- [ ] Replace `std::map<const char*, GeneratorParameter>` with fixed param array
- [ ] Generator writes to a passed-in `NotePool` staging buffer (not directly to pattern)
- [ ] `GeneratorManager` swaps staging buffer into generative layer atomically
- [ ] Port `EuclideanGenerator` to new API
- [ ] Remove `std::vector<bool>` temp allocation in Euclidean rhythm calc

Exec plan: `exec-plans/active/02-generator-redesign.md`

## Phase 3 — Engine & IO fixes

Goal: fix O(n) playback scan, O(n) MIDI queue pop, and serial in hot paths.

- [ ] `PlaybackEngine`: sorted notes + per-layer cursor index, O(1) tick advance
- [ ] `MidiIO`: replace linear queue with ring buffer for delayed events
- [ ] Remove `Serial.printf` from all hot paths; gate behind `SEQUENCER_DEBUG`
- [ ] Fix `MidiIO::allNotesOff` — 128 individual sends is fine but document why

Exec plan: `exec-plans/active/03-engine-io.md`

## Phase 4 — Scale & generative modes

Goal: expand scale system and add more generator algorithms.

- [ ] Add Chromatic, Minor, Major, Pentatonic, Phrygian scales
- [ ] Add Markov chain generator
- [ ] Add probability-grid generator
- [ ] Add cellular automata generator
- [ ] Seed/determinism controls per generator

## Backlog

- Multi-pattern chaining (song mode)
- Per-track MIDI channel routing
- Clock in (external sync)
