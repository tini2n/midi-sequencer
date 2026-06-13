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

Live in hardware.md

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
- Own length in steps **[OPEN: max length — 16? 32? 64?]**.
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

### 3.5 Length display convention

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
- Press unlit pad → create note at cursor's pitch lane, grid-aligned, default length.
- Press lit pad → select that note (focus on piano roll).
- Hold pad + turn encoder → edit that step directly.

Encoder map (no step held):

| E1 | E2 | E3 | E4 | E5 | E6 |
|---|---|---|---|---|---|
| Cursor X / scroll | Pitch lane Y / octave scroll | Velocity (selected) | Note length | Track length | BPM |

Encoder map (step held): E1–E4 retarget to **that note's** position (ticks) / pitch / velocity / length.

Shift+E4 = length in single ticks (fine). Shift+E1 = position nudge in ticks **[OPEN: confirm]**.

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

## 8. Decisions log

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

## 9. Open questions queue

1. Max track length, and page navigation on the piano roll when length > 16 steps (cursor window paging).
2. Euclid pitch behavior: rhythm-on-one-pitch vs. melodic spread across scale.
3. TRACK-held encoder mapping (channel, length per track).
4. Settings page full contents.
5. Shift hint bar on OLED when SHIFT held.
6. Count-in and loop-seam behavior for recording.
7. Audible ghost preview in v1 — feasible on the scheduler?
8. Save/load — does v1 persist anything (SD/flash)?