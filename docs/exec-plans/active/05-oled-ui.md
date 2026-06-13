# Exec Plan 05 — OLED UI

**Status:** IN PROGRESS — core wired, piano roll rendering active  
**Goal:** SSD1322 256×64 OLED showing sequencer state. Piano roll as primary view.

---

## What Is Done

- [x] `OledRenderer` — U8g2 wrapper, 8 MHz SPI, contrast 200, `velToGray(vel)` → gray 4–15
- [x] `UICtx` — read-only snapshot (`Pattern&`, `Transport&`, `SequencerMode&`, `micros()`, `settingsMode`)
- [x] `ScreenManager` — 30 fps cap, dirty flag, routes to `PianoRollView` or `SettingsView`
- [x] `SettingsView` — BPM display, K1 hint
- [x] `Viewport` — `tickStart`, `tickSpan`, `pitchBase` for piano roll pan
- [x] `PianoRollView` — primary view (replaces StepGridView from original plan)
  - Header: inverted (gray-15 fill + gray-0 text), track / BPM / page / transport
  - Piano key sidebar: 14px wide, white keys gray-9 filled, black keys dim label
  - Grid: vertical dotted beat/bar markers (bars gray-6, beats gray-3)
  - Notes: `drawBox` at correct tick/pitch position, fill = `velToGray(vel)`
  - Playhead: vertical bar at current transport tick
  - Viewport driven by `cursor.getPage()` (time) and `cursor.getEditPitch()` (pitch centre)
- [x] Wired into `App::setup()` and `App::update()`
- [x] `while (millis() < 600) {}` before `kb_.begin()` for PCF8575 power-on race

---

## Layout — PianoRollView (256×64)

```
┌──────────────────────────────────────────────────────┐  y=0
│ A  120.0BPM  PG:1/1  ▶                              │  8px header (inverted)
├─────┬────────────────────────────────────────────────┤  y=8
│  C5 │  ░░  ████            ████  ░░░░                │
│  B4 │                                                │  56px grid (9 lanes × 6px)
│  A#4│                ░░░░                            │
│  A4 │  ████                      ████                │
│  G#4│                                                │
│  G4 │        ████  ░░░░  ████                        │
│  F#4│                                                │
│  F4 │  ░░          ████                              │
│  E4 │                                                │
└─────┴────────────────────────────────────────────────┘  y=64
 14px          242px grid
```

Constants: `HEADER_H=8`, `LABEL_W=14`, `GRID_W=242`, `LANE_H=6`, `numLanes()=9`.  
Font for pitch labels: `u8g2_font_u8glib_4_tf` (4px wide, fits in 6px lane).  
Font for header: `u8g2_font_5x7_tf`.

---

## What Remains

- [ ] **Verify note rendering on hardware** — confirm notes visible at correct positions with real patterns
- [ ] **Step grid view** — a simpler 16-step toggle grid (Digitakt-style) may be wanted as an alternative view; deferred until piano roll is fully validated
- [ ] **GenerativeView** — shows generator params + trigger button; depends on Phase 4 generator subsystem
- [ ] **Playhead dirty optimisation** — currently `screenMgr_.markDirty()` called every `App::update()`; should only trigger on step boundary crossing to reduce SPI traffic

---

## Dirty Flag Strategy (pending)

Only redraw on actual state changes:

```cpp
// In ScreenManager or App::update():
uint8_t curStep = playTick / timebase::ticksPerStep(pat_.grid);
if (curStep != lastStep_) { lastStep_ = curStep; screenMgr_.markDirty(); }
```

Keyboard / encoder handlers already call `screenMgr_.markDirty()` on input — that part is correct.

---

## Phase 6 Stub

GenerativeView will display:
- Active generator name + short param list
- K1–K4 mapped to generator params
- CTL button to trigger generate
- Rendered generative notes overlaid on mini step grid (dim gray)
