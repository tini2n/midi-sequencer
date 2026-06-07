# Exec Plan 05 — OLED UI

**Status:** IN PROGRESS — v1 wired, pending hardware test  
**Goal:** SSD1322 256×64 display showing sequencer state. Piano roll as primary view. Velocity as native grayscale brightness. Max rendering efficiency.

---

## Display Hardware

**SSD1322 NHD 256×64, 4-bit grayscale, 4-wire HW SPI**  
Pins: CS=10, DC=9, RST=8 (Teensy SPI: SCK=13, MOSI=11)  
U8g2 driver: `U8G2_SSD1322_NHD_256X64_F_4W_HW_SPI` (F = full buffer = 8 KB in RAM1)

Gray levels: 0 (off) … 15 (full brightness). `u8g2.setDrawColor(n)` maps directly to hardware — no pixel stippling required. Velocity is drawn as actual gray luminance.

---

## Architecture

```
App
 ├── OledRenderer        (src/ui/oled_renderer.hpp)
 │    └── U8g2 SSD1322 — buffer management, velToGray(), primitive helpers
 ├── ScreenManager       (src/ui/screen_manager.hpp)
 │    ├── StepGridView   (src/ui/views/step_grid_view.hpp/.cpp) — default
 │    └── SettingsView   (src/ui/views/settings_view.hpp)      — CTL 6 active
 └── UICtx               — read-only snapshot passed to draw()
```

**No heap anywhere.** All view objects are concrete members of `ScreenManager` (stack/static). No `IView*` polymorphism — views are called directly via a switch on `ScreenId`. Virtual dispatch is unnecessary overhead for 2–3 fixed views.

---

## ScreenId & Mode Mapping

```cpp
enum class ScreenId : uint8_t {
    StepGrid  = 0,   // default — always shown
    Settings  = 1,   // CTL 6 active
};
```

| App state | ScreenId | Notes |
|-----------|----------|-------|
| Normal | StepGrid | Hold-step state shown as in-view overlay, not a separate screen |
| settingsMode_ = true | Settings | Full screen BPM control |

Hold-step editing is **an overlay within StepGrid**, not a separate screen. When `cursor.getHeldStep() >= 0`, the footer changes to show that note's properties instead of the edit defaults.

---

## UICtx — Drawing Context

```cpp
struct UICtx {
    const Pattern&    pat;       // full pattern (tracks, steps, tempo)
    const CursorMode& cursor;    // page, track, held step, edit pitch/vel/len
    const Transport&  transport; // isRunning(), current tick
    uint32_t          playTick;  // current playhead tick
    uint32_t          now;       // micros() for animation
    bool              settingsMode;
};
```

`UICtx` is constructed in `App::update()` and passed down. All fields are `const&` — views never mutate model state.

---

## Screen Layout — StepGrid (256×64)

```
┌────────────────────────────────────────────────────────────────────────┐  y=0
│ TRK:A  PG:1/1   120.0 BPM  16 steps          ▶                        │  8px header
├────────────────────────────────────────────────────────────────────────┤  y=8
│  ┌──┐┌──┐┌──┐┌──┐┌──┐┌──┐┌──┐┌──┐ ┌──┐┌──┐┌──┐┌──┐┌──┐┌──┐┌──┐┌──┐  │
│  │██││  ││██││  ││██││  ││  ││  │ │██││  ││  ││██││  ││██││  ││  │  │  │  40px step grid
│  │██││  ││▓▓││  ││░░││  ││  ││  │ │██││  ││  ││██││  ││▓▓││  ││  │  │  │
│  └──┘└──┘└──┘└──┘└──┘└──┘└──┘└──┘ └──┘└──┘└──┘└──┘└──┘└──┘└──┘└──┘  │
├────────────────────────────────────────────────────────────────────────┤  y=48
│  C4  ─────────── vel:100 ─────────── len:1 ──────────────────── 1/16  │  8px play footer
│  [hold step → K2:pitch K3:vel K4:dur K1:nudge]                        │  8px hint (dim)
└────────────────────────────────────────────────────────────────────────┘  y=64
```

### Step Block (15×38px, 1px gap between blocks = 16px pitch)

256px / 16 steps = 16px per slot. Each block: 15px wide, gap 1px.  
Block height: 40px (rows y=8 to y=47).

| State | Fill | Gray level |
|-------|------|-----------|
| Empty (no note) | outline only (1px border) | border: 3 |
| Active note | filled rect | `velToGray(vel)` → 4–15 |
| Active + playing | inverted (black on white) | bg:15, content:0 |
| Held step | thick border (2px) | border: 15 |
| Beyond pattern length | no outline, pixel dot | 2 |

### Velocity → Gray

```cpp
static uint8_t velToGray(uint8_t vel) {
    // vel 1–127 → gray 4–15 (always visible but dim for low vel)
    return uint8_t(4 + (uint16_t(vel) * 11u) / 127u);
}
```

### Header (y=0, h=8)

```
TRK:A  PG:1/1  120.0 BPM  16st  ▶
```

- Track: 0→A, 1→B (single char saves space)
- Page: current+1 / total pages (steps/16, rounded up)
- BPM: 3–4 chars (`120.0`)
- Steps: count
- Transport: `▶` (playing), `■` (stopped)
- Font: `u8g2_font_5x7_tf` (5px wide, 7px tall — fits in 8px row)

### Footer (y=48, h=16) — Normal mode

```
C4  vel:100  len:1
```

Shows current edit defaults. Font: `u8g2_font_5x7_tf`.

### Footer (y=48, h=16) — Hold-step mode (`cursor.getHeldStep() >= 0`)

```
STEP 3 ▸ F#3  vel:85  dur:2  nudge:+4
```

Replaces edit defaults. Step number + held note properties. Gray hint text for encoder labels.

---

## Screen Layout — Settings (256×64)

```
┌────────────────────────────────────────────────────────────────────────┐
│  ── SETTINGS ──────────────────────────────────────────────────────── │
│                                                                         │
│         BPM                                                             │
│        120.0          K1: turn = ±0.5   press = reset                  │
│                                                                         │
│  ─────────────────────────────────────────────────────────────────── │
│  K2–K8: reserved                                                        │
└────────────────────────────────────────────────────────────────────────┘
```

BPM in large font (`u8g2_font_logisoso16_tf` or similar 16px font). K1 hint dim gray (color 6).

---

## Rendering Optimization

### 1. Full-buffer mode (mandatory)

`F` suffix in U8g2 constructor = 8 KB framebuffer in RAM1. Flow:

```
clearBuffer() → drawAll() → sendBuffer()
```

One SPI transfer per frame (no page-loop overhead). At SPI 24 MHz, `sendBuffer()` for 256×64×4bpp takes ~5.5 ms. At 30 MHz: ~4.4 ms.

### 2. Frame rate cap + dirty flag

```cpp
bool     dirty_{true};
uint32_t lastDrawUs_{0};
static constexpr uint32_t kFrameIntervalUs = 33333; // 30 fps
```

`draw()` is a no-op unless `dirty_` is set AND `(now - lastDrawUs_) >= kFrameIntervalUs`. Events that set `dirty_`:
- Encoder rotation → edit value changed
- Note toggled
- Step count changed
- Transport state changed (play/stop)
- Playhead crossed a step boundary
- Settings mode toggled

### 3. Playhead dirty detection

Don't set dirty every tick (1 kHz). Detect step boundary crossing:

```cpp
uint8_t  lastPlayStep_{255};

uint8_t curStep = playTick / ticksPerStep;
if (curStep != lastPlayStep_) {
    lastPlayStep_ = curStep;
    dirty_ = true;
}
```

At 120 BPM, a 1/16 step = 125 ms → at most 8 redraws/sec from playhead alone.

### 4. No per-pixel loops in hot path

Step blocks are `drawBox()` calls (rectangle fill = single SSD1322 write command). Only the grayscale color changes between calls. Avoid inner pixel loops — the legacy stipple helpers are not needed on SSD1322.

### 5. Viewport is optional for StepGrid

The `Viewport` (pan/zoom) is the piano roll's domain. StepGrid derives its view from `cursor.getPage()` and `pat.steps`. Viewport is passed through but unused by StepGrid — it stays ready for a future PianoRollView.

---

## File Structure

```
src/ui/
  oled_renderer.hpp      — OledRenderer class: begin(), clear(), send(), velToGray()
  screen_manager.hpp     — ScreenManager: holds views, dirty flag, frame cap, draw()
  views/
    step_grid_view.hpp   — StepGridView: layout constants + draw(U8G2&, UICtx&)
    step_grid_view.cpp
    settings_view.hpp    — SettingsView: BPM display, draw(U8G2&, UICtx&)
```

No `.cpp` files for settings view (small enough to keep header-only).

---

## Integration into App

### app.hpp additions

```cpp
#include "ui/oled_renderer.hpp"
#include "ui/screen_manager.hpp"

class App : public IEncoderHandler {
    // ... existing members ...
    OledRenderer  oled_;
    ScreenManager screenMgr_;
};
```

### app.cpp setup()

```cpp
oled_.begin();
screenMgr_.begin(&oled_);
```

### app.cpp update()

```cpp
// Build context
UICtx ctx{ pat_, cursor_, tx_, loop_.playTick(), micros(), settingsMode_ };

// Notify screen manager of state that drives dirty detection
screenMgr_.setScreen(settingsMode_ ? ScreenId::Settings : ScreenId::StepGrid);
screenMgr_.update(ctx);  // dirty detection
screenMgr_.draw(ctx);    // draws only if dirty + frame cap
```

`loop_.playTick()` — needs to be added to `RunLoop` as a passthrough from `Transport`.

---

## Implementation Steps

- [ ] 1. `src/ui/oled_renderer.hpp` — `begin()`, `clear()`, `send()`, `velToGray()`, `u8g2_` member
- [ ] 2. `src/ui/views/step_grid_view.hpp/.cpp` — header + step blocks + footer (normal + held-step)
- [ ] 3. `src/ui/views/settings_view.hpp` — BPM large text, K hint
- [ ] 4. `src/ui/screen_manager.hpp` — dirty flag, frame cap, `update()`, `draw()`
- [ ] 5. Wire `OledRenderer` + `ScreenManager` into `app.hpp/.cpp`
- [ ] 6. Expose `playTick` from `RunLoop` or `Transport` for `UICtx`
- [ ] 7. Build + upload; verify frame rate and step grid layout
- [ ] 8. (Optional) `PianoRollView` as K6 view — reuse legacy piano_roll.cpp logic with gray fills

---

## Open Questions (resolve before step 2)

| Question | Options |
|----------|---------|
| Step block gap: 1px fixed or per-step bar line? | 1px gap cleaner; bar lines (beat groups of 4) can be drawn at the gap position |
| Track A/B shown together or only active track? | One active track fills the grid. Two tracks = 20px each, 8px gap — complex, defer |
| Beyond-pattern steps: dim or hidden? | Dim (gray 1 outline) — shows available range |
| Font for header: 5×7 or smaller? | 5×7 just fits; 4×6 if needed |
| Playhead: full vertical bar or top indicator? | Full bar (inverted step block) — most visible |
