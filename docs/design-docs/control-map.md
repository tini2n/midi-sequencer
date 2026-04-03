# Control Map

Hardware surface: **Teensy 4.1** + PCF8575 matrix keyboard (16 step buttons + 8 CTL buttons) + 8 rotary encoders.

---

## CTL Buttons

```
[REC][PLAY][STOP][PG+][MODE][TRK][SET][SHF]
  0    1     2    3    4     5    6    7
```

| # | Label | Action | Shift combo |
|---|-------|--------|-------------|
| 0 | REC   | Placeholder — real-time recording TBD | — |
| 1 | PLAY  | Play / pause toggle | — |
| 2 | STOP  | Stop + silence all tracks | — |
| 3 | PG+   | Next page (16 steps forward) | Shift+PG+ = previous page |
| 4 | MODE  | Cycle keyboard mode (sequencer only for now) | — |
| 5 | TRK   | Toggle active track 0 ↔ 1 | — |
| 6 | SET   | Toggle Settings mode | — |
| 7 | SHF   | Shift modifier (held) | — |

---

## Encoders — First Layer (Sequencer Mode)

Encoders group into three categories:

```
Navigation  │ Feel        │ Structure
K1  K2  K3  │ K4  K5  K6 │ K7  K8
```

| Encoder | Category | Turn | Press |
|---------|----------|------|-------|
| K1 (0) | Navigation | Page offset ±1 | Reset page to 0 |
| K2 (1) | Navigation | Edit pitch ±1 semitone | Reset pitch to C4 (60) |
| K3 (2) | Navigation | TODO | — |
| K4 (3) | Navigation | TODO — zoom (Phase 5) | — |
| K5 (4) | Structure | Step count ±1 (hold K5 + turn = ±16) | — |
| K6 (5) | Structure | TODO | — |
| K7 (6) | Feel | TODO | — |
| K8 (7) | Feel | TODO | — |

---

## Encoders — Settings Mode (CTL 6 active)

Pressing CTL 6 enters Settings mode. Serial prints `[SET] on  BPM=xxx`.

| Encoder | Action |
|---------|--------|
| K1 (0) | BPM ±0.5 per detent; press = reset to 120 |
| K2–K8  | Reserved (future: MIDI channel, clock, etc.) |

Press CTL 6 again to exit. Serial prints `[SET] off`.

---

## Hold-Step Note Editing

In Sequencer mode, **holding a step button while turning an encoder** edits that note's properties:

| Encoder | Edits |
|---------|-------|
| K2 (1) | Pitch ±1 semitone (0–127) |
| K3 (2) | Velocity ±1 (1–127) |
| K4 (3) | Micro-offset: nudge note's tick position ±1 |

**Behavior:**
- Step press → note toggles on immediately (note is created/removed)
- While held + encoder moved → note property updates live; track grid reprints after each delta
- Release → held state cleared

---

## Shift Combos (CTL 7 held)

| Button | Action |
|--------|--------|
| Shift + CTL 3 | Previous page |
| Shift + CTL 0 | Clear selected step (CursorMode) |
| Shift + CTL 1 | Copy selected step |
| Shift + CTL 2 | Paste to selected step |

---

## Step Buttons (16)

In Sequencer mode, buttons 0–15 map to step slots on the active track:

```
actual_step = button + pageOffset × 16
```

Press = toggle note on/off at that step using current edit pitch.

---

## Open / TODO

| Item | Notes |
|------|-------|
| K3 encoder function | TBD |
| K4 encoder function | Zoom — deferred to Phase 5 (screen integration) |
| K6–K8 encoder functions | TBD — candidates: grid resolution, velocity default, swing |
| REC (CTL 0) | Real-time recording deferred to future phase |
| MODE (CTL 4) | Only sequencer mode exists; additional modes TBD |
| Settings: K2–K8 | Additional settings params TBD |
