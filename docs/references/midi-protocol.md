# MIDI Protocol Reference

Minimal reference for the sequencer implementation.

## Message Bytes

| Message | Status | Data 1 | Data 2 |
|---------|--------|--------|--------|
| Note Off | 0x80 + ch | pitch | velocity |
| Note On | 0x90 + ch | pitch | velocity (0 = note off) |
| Control Change | 0xB0 + ch | CC# | value |
| Clock | 0xF8 | — | — |
| Start | 0xFA | — | — |
| Continue | 0xFB | — | — |
| Stop | 0xFC | — | — |

Channel is 0-indexed in the status byte (`ch - 1`). Our `MidiIO` handles this.

## Clock

MIDI clock = 24 pulses per quarter note (PPQN = 24 from the MIDI spec perspective).
Our internal PPQN = 96. We send a MIDI clock byte every 4 internal ticks (`clkDiv_ == 4`).

## CC 123 — All Notes Off

Tells receivers to release all sounding notes. Should be sent on stop.
We send this + optionally CC 120 (All Sound Off) in `MidiIO::sendAllNotesOffCC`.

## Note Timing

At 120 BPM:
- 1 quarter note = 500 ms
- 1/16 note = 125 ms = 24 internal ticks
- 1 internal tick = 500 ms / 96 ≈ 5.2 ms

## PPQN Math

```
ticksPerStep(grid) = (PPQN * 4) / grid
```
Examples (PPQN = 96):
- 1/4  (grid=4)  → 96 ticks
- 1/8  (grid=8)  → 48 ticks
- 1/16 (grid=16) → 24 ticks
- 1/32 (grid=32) → 12 ticks
