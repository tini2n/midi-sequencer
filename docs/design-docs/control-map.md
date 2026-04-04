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

## Encoders — Sequencer Mode (normal, no step held)

Two interaction layers on the same knobs depending on whether a step button is held.

```
K1       K2       K3       K4       K5
page     pitch    vel      length   steps
```

| Encoder | Turn | Press |
|---------|------|-------|
| K1 (0) | Page offset ±1 | Reset page to 0 |
| K2 (1) | Edit pitch ±1 semitone (0–127) | Reset pitch to C4 (60) |
| K3 (2) | Edit velocity ±1 (1–127) | Reset velocity to 100 |
| K4 (3) | Edit note length ±1 step (1–128) | Reset length to 1 step |
| K5 (4) | Step count ±1 (hold K5 + turn = ±16) | — |
| K6–K8  | Reserved | — |

---

## Encoders — Hold-Step Mode (step button held)

Hold any step button and turn an encoder to edit that note's properties live.
Every encoder shifts **one position left** compared to normal mode — K1 is always active.

```
K1       K2       K3       K4
pitch    vel      nudge    duration
```

| Encoder | Edits | Range |
|---------|-------|-------|
| K1 (0) | Tick nudge — shifts note's exact start position ±1 tick | 0–∞ |
| K2 (1) | Pitch ±1 semitone | 0–127 |
| K3 (2) | Velocity ±1 | 1–127 |
| K4 (3) | Note duration ±1 step | 1–128 steps |

**Behavior:**
- Step press → note toggles on immediately; held state begins
- While held + encoder turned → note updates live; track grid reprints after each delta
- Release → held state clears; no second toggle

---

## Encoders — Settings Mode (CTL 6 active)

Press CTL 6 to enter Settings mode (`[SET] on`). Press again to exit (`[SET] off`).

| Encoder | Action |
|---------|--------|
| K1 (0) | BPM ±0.5 per detent; press = reset to 120 |
| K2–K8  | Reserved (MIDI channel, clock source, swing — TBD) |

---

## Shift Combos (CTL 7 held)

| Button | Action |
|--------|--------|
| Shift + PG+ (CTL 3) | Previous page |
| Shift + REC (CTL 0) | Clear selected step |
| Shift + PLAY (CTL 1) | Copy selected step |
| Shift + STOP (CTL 2) | Paste to selected step |

---

## Step Buttons (16)

In Sequencer mode, buttons 0–15 map to step slots on the active track:

```
actual_step = button + pageOffset × 16
```

Press = toggle note on/off using current edit pitch, velocity, and length.

---

## Open / TODO

| Item | Notes |
|------|-------|
| K6–K8 encoder functions | Candidates: swing, grid resolution, MIDI channel |
| REC (CTL 0) | Real-time recording — future phase |
| MODE (CTL 4) | Only sequencer mode exists; additional modes TBD |
| Settings: K2–K8 | MIDI channel, clock source, swing TBD |
