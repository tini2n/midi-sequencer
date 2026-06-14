# MIDI Sequencer — UX Specification

**v0.1 (prototype scope)** · Teensy 4.1 · 2026-06-11

This document describes what the device does, its modes, and how it is used. It reflects decisions made during design sessions; open items are marked **[OPEN]**. Superseded ideas from the original concept doc (patterns, scenes, separate modulation mode) are intentionally out of v1 scope.

---

## 1. Concept

A polyphonic, performance-first MIDI sequencer. It produces no sound — only MIDI. The piano roll on the OLED is the single source of truth: it always shows what the focused track is playing. The 2×8 RGB pad grid is a chameleon surface: a 16-slot **time cursor** in sequencer view, a **piano keyboard** in KEYS layout.

Design principles:

- **Piano roll is home.** You never "leave" to use a feature; features arrive as overlays on top of it.
- **Tools, not modes.** Generation, quantization, and (later) groove/humanize share one lifecycle: pick → tweak → ghost preview → commit.
- **No hidden state.** Anything that changes behavior (record mode, layout, armed track) is visible in the OLED header.
- **Shift is the sub-function layer** (Digitakt-style). Primary press = primary action; Shift+X = related secondary action.

---

## 2. Hardware surface

| Module | Spec | Role |
|---|---|---|
| Pad grid | 16 RGB buttons, 2×8 | Time cursor / piano keyboard / track ops (while TRACK held) |
| Control buttons | 8 | PLAY, STOP, REC, SHIFT, GEN, KEYS, TRACK, SET |
| Encoders | 8 clickable (6 functional in current build — UI designed for 6) | Navigation + parameter editing |
| Display | OLED 256×64 mono | Piano roll + header + tool panels |
| MIDI | 1× IN, 1× OUT (3× OUT planned for production) | Clock + notes |

---

## 3. Data model & timing

### 3.1 Hierarchy (v1)

```
Project (implicit, single)
 └── Track ×4
      └── Notes
```

No patterns, no scenes in v1. Each track is one looping sequence.

### 3.2 Track

- Polyphonic MIDI sequence.
- Length: 1–128 steps (1 step = one 1/16 = 24 ticks). Stored absolutely in ticks; never affected by the view's GRID setting.
- Assigned to a MIDI channel.
- **Channel exclusivity rule:** a MIDI channel can have only one *active* track. If several tracks are assigned to the same channel, all but one are auto-muted. Unmuting one of them mutes the currently active one (last-unmuted-wins).

### 3.3 Note

```
Note { pitch, velocity, start_tick, length_ticks }
```

### 3.4 Timing grid

- Internal resolution: **96 PPQN** → **24 ticks per 16th-note step**.
- MIDI clock sync is a clean 4:1 (MIDI clock = 24 PPQN).
- Position and length are both tick-based → microtiming and off-grid recording come for free.
- Live recording captures **raw ticks** (no forced snap); quantization is a Tool applied afterwards (§6.3).

### 3.5 GRID, zoom & length — one lens, three jobs

Three concepts that must stay distinct, even though one knob (E4) drives the first:

1. **GRID** — the active resolution. Sets three things at once: the snap, the default length of newly created notes, and the zoom. Range 1/64 … 1/4. Bigger grid = each pad covers more time = view zooms out; smaller grid = zooms in. Changed with E4 when nothing is held.
2. **A note's own length** — stored per note in ticks, independent of GRID. Edited by holding that note and turning E4 (Shift+E4 = single ticks). **A note keeps its length when GRID/zoom changes** — zooming never resizes existing notes.
3. **Track length** — stored absolutely (§3.2). Never measured in "pages," because a page = 16 grid cells = a different duration at each zoom. E5 edits it in 1/16 steps; Shift+E5 in bars (×16).

The 16 pads always represent **16 buckets of the currently visible window** (per the cursor model), so their time-span follows GRID:

| GRID | Each pad covers | 16 pads span |
|---|---|---|
| 1/8 | 1/8 note | 2 bars (zoomed out) |
| 1/16 | 1/16 note | 1 bar (default) |
| 1/32 | 1/32 note | ½ bar (zoomed in) |

New-note length = one grid cell (couples to zoom by design). Existing-note length wins on zoom.

### 3.6 Length display convention

Show note lengths musically when clean (`1/16`, `1/8`, `1/8·`), otherwise as `steps:ticks` (e.g. `2:13`).

---

## 4. Control buttons

| Button | Press | Shift + press | Hold |
|---|---|---|---|
| PLAY | Start playback | *(reserved)* | — |
| STOP | Stop | *(candidate: clear / stop+rewind)* | — |
| REC | Arm/disarm recording | Toggle record mode OVR ↔ RPL | *(later: live-erase combos)* |
| SHIFT | — (modifier) | — | Activates shift layer |
| GEN | Open Tools overlay / **Commit** when open | *(later: jump to modulator-class tools)* | **Cancel** overlay without writing |
| KEYS | Toggle pads: cursor ↔ keyboard | Toggle keyboard sub-layout: piano ↔ scale-fold | — |
| TRACK | (tap) *(reserved)* | *(candidate: copy/paste track)* | Pads become track select/mute (§5.4) |
| SET | Settings page | *(reserved)* | — |

The Shift layer is built combo-by-combo; this table is the living registry. Rule: every Shift combo must be discoverable on screen when SHIFT is held **[OPEN: shift hint bar on OLED?]**.

Pad-level Shift combos (sequencer view):

| Action | Result |
|---|---|
| Step press | Toggle/select step at cursor |
| Step hold + encoder | Edit that step's parameter (§5.2) |
| Shift + step | Mute step (note stays, doesn't play) |
| Shift + encoder (pitch) | Octave jump instead of semitone |

---

## 5. Views & layouts

### 5.1 OLED layout (always)

- **Header (top strip):** BPM · focused track · play/REC state (`▶ ● OVR`) · root/scale · layout indicator.
- **Body:** piano roll — time on X, pitch lanes on Y. Notes are horizontal blocks; selected notes invert; velocity rendered as fill density/thickness.
- **Bottom strip (contextual):** tool panel when GEN overlay is open; otherwise minimal.

### 5.2 Sequencer view (home)

Pads = 16-slot time cursor mapped to the visible window.

Pad LED logic:
- **Bright** — at least one note at the selected pitch lane in that time bucket.
- **Dim** — bucket empty at that lane.
- **Playhead flash** — transport passing over the bucket.

Interactions:
- Press unlit pad → create note at cursor's pitch lane, at the bucket start, length = one grid cell.
- Press lit pad → select the note in that bucket (focus on piano roll).
- Hold pad + turn encoder → edit that note directly.

Encoder map (no note held):

| E1 | E2 | E3 | E4 | E5 | E6 |
|---|---|---|---|---|---|
| Cursor X (move by 1 step) | Pitch lane Y / octave scroll | Velocity (selected) | **GRID** (= zoom = default length) | Track length (1/16 steps) | BPM |

Navigation modifiers:
- **Shift+E1** → page by one screenful (16 buckets).
- **Shift+E5** → track length in bars (×16).

Encoder map (note held): E1–E4 retarget to **that note's** position (ticks) / pitch / velocity / length. Shift+E4 = length in single ticks. Shift+E1 (held) = position nudge in ticks.

> A "page" (16 buckets) is a viewing/navigation unit whose duration follows GRID — fine for jumping the view, never used to measure track length.

### 5.3 KEYS layout (pads as keyboard)

Entered with KEYS. Default sub-layout: **piano**. Shift+KEYS toggles piano ↔ scale-fold. Piano roll stays on screen.

**Piano layout** (one octave + next root):

```
Top:    ·   C#  D#  ·   F#  G#  A#  · 
Bottom: C   D   E   F   G   A   B   C'
```

**Scale-fold layout:** all 16 pads are scale degrees. Bottom row = degrees 1–8, top row = degrees 8–15 (two stacked octave rows of the selected scale).

Encoder map in KEYS:

| E1 | E2 | E3 | E4 |
|---|---|---|---|
| Root note | Scale | Octave | Pad velocity (the "brush") |

- Pads are not velocity-sensitive: E4 sets the velocity all pads play/record with.
- **Hold pad + turn E4** → edits the stored velocity of the note just recorded at that key. (MIDI can't change a sounding note's velocity after note-on; this edits the recorded data.)
- External MIDI keyboard input follows the same path (if enabled in Settings).

### 5.4 TRACK hold layer

While TRACK is held, pads remap:

- Bottom row pads 1–4 → **select/focus** track 1–4.
- Top row pads 1–4 → **mute/unmute** track 1–4 (respecting the channel-exclusivity rule).
- LED colors = per-track color identity; mute state shown dim/bright.

**[OPEN]** While TRACK held, encoders could edit focused-track properties (MIDI channel, length). Decide mapping.

### 5.5 Settings (SET)

v1 contents: BPM default, clock source (INT/EXT), MIDI channel per track, external-keyboard input on/off + channel, pad default velocity. **[OPEN: full list]**

---

## 6. Recording (the "looper")

There is no looper *mode* — recording is a state (REC armed) that composes with the pad layout:

| KEYS | REC | Result |
|---|---|---|
| off | off | Step sequencing |
| on | off | Jam over the sequence, nothing written |
| on | on | **Live loop recording** from pads |
| off | on | Step-record / record from external MIDI |

Rules:
- Recording captures raw ticks (no input quantize).
- **Record modes:** `OVR` (overdub — layers onto existing notes) / `RPL` (replace — erases notes as the playhead passes while keys are held). Toggled with Shift+REC; current mode always shown in header.
- Quantize afterwards via the Quantize tool (§7).

**[OPEN]** Count-in before record? Loop-boundary behavior (note held across the loop seam)?

---

## 7. Tools overlay (GEN)

One button, one lifecycle, many tools. Press GEN in sequencer view:

1. Bottom strip of OLED becomes the **tool panel**; piano roll stays visible.
2. **E1 = tool select.** v1 list: `Euclid`, `Quantize`. (Future: Humanize, Swing/Groove, Mutate, Contour, Markov…)
3. **E2–E5 = tool parameters** (per tool).
4. **E6 = commit policy** where relevant: Replace / Fill Rests / Ornament / Subtract.
5. Result renders live as **ghost notes** (dotted/dim) over existing notes. If the track is playing, preview is audible non-destructively **[OPEN: confirm audible preview in v1]**.
6. **GEN press = Commit** (writes to track). **GEN hold = Cancel** (exit, nothing written).

### 7.1 Euclid (v1)

| Param | Range | Notes |
|---|---|---|
| Steps (L) | 1–track length | pattern length the algorithm fills |
| Fills (k) | 0–L | number of hits |
| Rotation (r) | 0–L−1 | shifts the pattern |
| Pitch / range | — | **[OPEN]** single pitch at cursor lane vs. scale-spread |

### 7.2 Quantize (v1)

| Param | Range |
|---|---|
| Grid | 1/4 · 1/8 · 1/16 |
| Strength | 0–100% |

Strength moves each note proportionally toward the grid — 100% = hard snap.

---

## 8. Performance & architecture rules (engine constraints)

Target: Teensy 4.1 (600 MHz Cortex-M7, 1 MB RAM). Headroom is large; lag only appears if these rules are broken.

- **Timing logic never touches the display.** The tick handler schedules MIDI only — no draw calls, no allocation. (Scheduling load for hundreds of notes at 96 PPQN is <1% CPU.)
- **No dynamic allocation in the audio/timing path.** Notes live in a fixed array; a note ≈ 6–8 bytes, so thousands of notes fit comfortably in RAM.
- **Renderer culls to the visible tick window before iterating notes.** Render cost is then bounded by screen width (~256 px), not by how many notes the track holds. A fully-filled 128-step track only ever draws the handful of notes currently on screen.
- **Keep notes sorted by start tick** (or maintain a small active-note window) so the scheduler scans only notes near the playhead.
- **Redraw at a frame cadence (~30–60 fps), not per tick.** The real cost is pushing the framebuffer to the OLED over SPI (~1–2 ms/frame), so render only when something changed.

Consequence: 4 tracks fully filled at fine resolution is a non-event; the device has room for far more before the Teensy notices.

---

## 9. Decisions log

| # | Decision | Status |
|---|---|---|
| 1 | v1 hierarchy: tracks only, no patterns/scenes | ✓ |
| 2 | 4 tracks | ✓ |
| 3 | Generator = overlay with ghost preview, not a mode | ✓ |
| 4 | 8 buttons: PLAY STOP REC SHIFT GEN KEYS TRACK SET | ✓ |
| 5 | Dedicated STOP (no play/stop toggle) | ✓ |
| 6 | Shift = Digitakt-style sub-function layer | ✓ |
| 7 | 96 PPQN, tick-based note position & length | ✓ |
| 8 | Record raw, quantize as a post Tool | ✓ |
| 9 | Overdub + Replace, toggled Shift+REC, shown in header | ✓ |
| 10 | Tools overlay unifies generators + processors | ✓ |
| 11 | Pad velocity: encoder "brush" + hold-pad fine edit | ✓ |
| 12 | Octave shift via encoder | ✓ |
| 13 | KEYS default = piano layout | ✓ |
| 14 | Track length 1–128 steps, stored in ticks | ✓ |
| 15 | GRID (E4) = snap + default length + zoom; pads = 16 buckets of visible window | ✓ |
| 16 | New note = one grid cell; existing notes keep length on zoom | ✓ |
| 17 | E1 = move 1 step, Shift+E1 = page (16 buckets); length never measured in pages | ✓ |
| 18 | Engine rules: cull-before-render, no alloc/draw in timing path | ✓ |

## 10. Open questions queue

1. Euclid pitch behavior: rhythm-on-one-pitch vs. melodic spread across scale (a `spread` param — distribution rule: deterministic cycle vs. seeded random).
2. TRACK-held encoder mapping (channel, length per track).
3. Settings page full contents.
4. Shift hint bar on OLED when SHIFT held.
5. Recording details parked for hardware testing: count-in, loop-seam behavior, touch-to-freeze-window anti-drift.
6. Audible ghost preview in v1 — feasible on the scheduler?
7. Save/load — does v1 persist anything (SD/flash)?
8. Sequencer editing still to design: polyphony in one bucket (select/cycle), note deletion, range selection, empty-bucket context actions.