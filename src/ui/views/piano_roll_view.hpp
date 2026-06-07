#pragma once
#include <U8g2lib.h>
#include "../ui_ctx.hpp"
#include "../viewport.hpp"
#include "../../model/note_pool.hpp"

// Primary view: piano roll for the active track.
//
// Layout (256 × 64):
//   y= 0..7   header  — track, BPM, transport, step count, page
//   y= 8..63  piano roll — 14px keyboard labels + 242px note grid
//
// Each pitch lane is LANE_H=6px tall.
// Notes are filled rectangles whose gray level encodes velocity (4–15).
// Grid lines mark steps (dim), beats (dashed), and bars (solid).
// Playhead is a bright vertical line at the current play tick.
//
// Viewport is driven by the cursor each frame:
//   K1 (page)       → tickStart = page * 16 * ticksPerStep, tickSpan = 16 steps
//   K2 (editPitch)  → pitchBase centres on editPitch
class PianoRollView {
public:
    void begin();
    void draw(U8G2& gfx, const UICtx& ctx);

    Viewport& viewport() { return viewport_; }

private:
    // ── Layout ────────────────────────────────────────────────────────────────
    static constexpr uint8_t  HEADER_H = 8;    // 8px header — u8g2_font_5x7_tf baseline at y=7
    static constexpr uint8_t  LABEL_W  = 14;   // piano keyboard column width
    static constexpr uint16_t GRID_W   = 256 - LABEL_W;   // 242
    static constexpr uint8_t  GRID_Y   = HEADER_H;        // 8
    static constexpr uint8_t  GRID_H   = 64 - HEADER_H;   // 56
    static constexpr uint8_t  LANE_H   = 6;    // px per semitone lane (numLanes = 9)
    // ─────────────────────────────────────────────────────────────────────────

    Viewport viewport_{};

    void drawHeader   (U8G2& gfx, const UICtx& ctx);
    void drawPianoKeys(U8G2& gfx, const UICtx& ctx);
    void drawGrid     (U8G2& gfx, const UICtx& ctx);
    void drawNotePool (U8G2& gfx, const NotePool<128>& pool);
    void drawPlayhead (U8G2& gfx, const UICtx& ctx);

    // Tick → x pixel within the grid (may be outside LABEL_W..LABEL_W+GRID_W).
    int32_t xFromTick (uint32_t tick)  const;
    // Pitch → y pixel (top-left of that lane's box).
    int16_t yFromPitch(uint8_t  pitch) const;
    // Number of fully visible pitch lanes.
    uint8_t numLanes() const { return GRID_H / LANE_H; }

    static bool isBlackKey(uint8_t midi);
};
