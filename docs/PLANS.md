# PLANS.md — Implementation Roadmap

Phases complete in order. Do not start a phase until the previous one is verified on hardware.

---

## ✓ Phase 1 — Data Model

Fixed-size `NotePool<N>`, `LayeredTrack`, `Pattern`, `Scale`, `Viewport`. No heap. Done.

## ✓ Phase 2 — Core MIDI Pipeline

`TickScheduler` ISR → `RunLoop` → `Transport` → `PlaybackEngine` → `MidiIO` → Serial1. Cursor-based O(1) scan. Done.

## ✓ Phase 3 — Step Sequencer & Control Surface

`SequencerMode`, `MatrixKB` (PCF8575), `EncoderManager`, `SerialMonitor`. 16-button step editing, 2 tracks, page/pitch/velocity/length encoders. Done.

## → Phase 4 — Generator Subsystem

Non-destructive Euclidean generator into `generative` layer. Fixed-param array, staging buffer, atomic swap. Plan: `exec-plans/active/04-generator-redesign.md`.

- [ ] Generator base + fixed param array
- [ ] EuclideanGenerator port (no heap, no std::map)
- [ ] GeneratorManager with staging buffer + atomic swap
- [ ] Wire `applyPendingSwap` into RunLoop

## → Phase 5 — OLED UI (in progress)

SSD1322 256×64 piano roll. Plan: `exec-plans/active/05-oled-ui.md`.

- [x] OledRenderer, UICtx, ScreenManager, Viewport
- [x] PianoRollView — header, grid, piano keys, notes, playhead
- [x] SettingsView — BPM display
- [ ] Verify note rendering on hardware with real patterns
- [ ] Playhead dirty optimisation (step-boundary only)
- [ ] GenerativeView (depends on Phase 4)

## Phase 6 — Generators & Scale Modes

- [ ] Markov chain generator
- [ ] Probability-grid generator
- [ ] Cellular automata generator
- [ ] Seed / determinism controls per generator
- [ ] Chromatic scale (if needed)

## Backlog

- Multi-pattern chaining (song mode)
- Per-track MIDI channel routing from UI
- External clock sync (MIDI clock in)
