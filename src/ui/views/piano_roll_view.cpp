#include "piano_roll_view.hpp"
#include "../../types.hpp"   // timebase::ticksPerStep

// ── Helpers ────────────────────────────────────────────────────────────────

// MIDI velocity 0–127 → gray level 1–15 (1 = barely-lit pp, 15 = full ff).
static uint8_t velToGray(uint8_t vel) {
    return uint8_t(1 + (uint16_t(vel) * 14u) / 127u);
}

static void pitchName(uint8_t p, char out[5]) {
    static const char* N[12] = {"C","C#","D","D#","E","F","F#","G","G#","A","A#","B"};
    snprintf(out, 5, "%s%d", N[p % 12], p / 12);
}

bool PianoRollView::isBlackKey(uint8_t midi) {
    uint8_t k = midi % 12;
    return k == 1 || k == 3 || k == 6 || k == 8 || k == 10;
}

// ── Coordinate math ────────────────────────────────────────────────────────

int32_t PianoRollView::xFromTick(uint32_t tick) const {
    return LABEL_W + int32_t(
        int64_t(int64_t(tick) - int64_t(viewport_.tickStart))
        * int64_t(GRID_W) / int64_t(viewport_.tickSpan));
}

int16_t PianoRollView::yFromPitch(uint8_t pitch) const {
    int16_t lane = int16_t(pitch) - int16_t(viewport_.pitchBase);
    return int16_t(GRID_Y + GRID_H) - int16_t(lane + 1) * int16_t(LANE_H);
}

// ── Begin ──────────────────────────────────────────────────────────────────

void PianoRollView::begin() {
    viewport_.tickStart = 0;
    viewport_.tickSpan  = 384;
    viewport_.pitchBase = 48;
}

// ── Draw sub-routines ──────────────────────────────────────────────────────

void PianoRollView::drawHeader(U8G2& gfx, const UICtx& ctx) {
    // Black header bar with white text.
    // u8g2_font_5x7_tf baseline at y=7 (HEADER_H-1): glyphs fill y=1..7 inside the 8px row.
    gfx.setDrawColor(0);
    gfx.drawBox(0, 0, 256, HEADER_H);   // clear header region to black

    gfx.setFont(u8g2_font_5x7_tf);
    gfx.setDrawColor(15);   // white text on black background

    // Layout: "120  T1  1/16  2:3 / 4:0  >"
    //   BPM | focused track | GRID | cursor bar:beat / track-length bar:beat | transport
    const uint8_t  t       = ctx.sequencer.getTrack();
    const uint32_t cur     = ctx.sequencer.getCursorTick();
    const uint32_t len     = ctx.pat.tracks[t].lenTicks;
    char buf[64];
    snprintf(buf, sizeof(buf), "%ld  T%u  1/%u  %d:%d / %ld:%ld  %s",
             long(ctx.pat.tempo + 0.5f),
             unsigned(t + 1),
             unsigned(ctx.pat.grid),
             seq::barOf(cur), seq::beatOf(cur),
             long(len / seq::TICKS_PER_BAR),
             long((len % seq::TICKS_PER_BAR) / seq::TICKS_PER_BEAT),
             ctx.transport.isRunning() ? ">" : "[]");
    gfx.drawStr(2, HEADER_H - 1, buf);
    gfx.setDrawColor(15);
}

// Piano keyboard sidebar — font u8g2_font_u8glib_4_tf (4px wide, 5px cap height).
// Baseline = y0 + LANE_H - 1 so the glyph sits flush with the lane bottom.
// White keys: gray-9 fill with inverted (gray-0) text.
// Black keys: no fill, dim gray-6 text.
void PianoRollView::drawPianoKeys(U8G2& gfx, const UICtx& ctx) {
    (void)ctx;
    gfx.setFont(u8g2_font_u8glib_4_tf);
    const uint8_t lanes = numLanes();

    for (uint8_t i = 0; i < lanes; i++) {
        const uint8_t p  = viewport_.pitchBase + i;
        const int16_t y0 = yFromPitch(p);
        const uint8_t h  = LANE_H - 1;

        char txt[5];
        pitchName(p, txt);

        if (!isBlackKey(p)) {
            gfx.setDrawColor(9);
            gfx.drawBox(0, y0, LABEL_W - 1, h);
            gfx.setDrawColor(0);
            gfx.drawStr(1, y0 + LANE_H - 1, txt);
            gfx.setDrawColor(15);
        } else {
            gfx.setDrawColor(6);
            gfx.drawStr(1, y0 + LANE_H - 1, txt);
            gfx.setDrawColor(15);
        }
    }
}

// Grid: vertical dotted markers, three densities for a bar > beat > step hierarchy.
// Bars  (every 16 cells): gray-6, a dot every 3 px.
// Beats (every  4 cells): gray-3, a dot every 3 px.
// Steps (each cell, incl. the 3 between beats): gray-2, a dot every 6 px (half density).
void PianoRollView::drawGrid(GrayCanvas& g, const UICtx& ctx) {
    const uint32_t stepT    = timebase::ticksPerStep(ctx.pat.grid);
    const uint32_t beatT    = stepT * 4;
    const uint32_t barT     = stepT * 16;
    const int32_t  gx1      = LABEL_W + GRID_W;
    const uint32_t trackLen = ctx.pat.tracks[ctx.sequencer.getTrack()].lenTicks;
    const uint32_t vpEnd    = viewport_.tickStart + viewport_.tickSpan;

    uint32_t s = viewport_.tickStart / stepT;
    for (;;) {
        uint32_t tick = s * stepT;
        if (tick >= vpEnd || tick >= trackLen) break;  // stop at window edge or track end

        int32_t x = xFromTick(tick);
        if (x > LABEL_W && x < gx1) {
            uint8_t gray;
            int16_t dotStep;   // vertical spacing between dots
            if      (tick % barT  == 0) { gray = 6; dotStep = 3; }
            else if (tick % beatT == 0) { gray = 3; dotStep = 3; }
            else                        { gray = 2; dotStep = 6; }  // step subdivisions
            for (int16_t y = GRID_Y; y < GRID_Y + GRID_H; y += dotStep)
                g.setPixel(x, y, gray);
        }
        s++;
    }
}

void PianoRollView::drawNotePool(GrayCanvas& g, const NotePool<128>& pool) {
    const int32_t  gx0   = LABEL_W;
    const int32_t  gx1   = LABEL_W + GRID_W;
    const uint8_t  lanes = numLanes();
    const uint32_t vpEnd = viewport_.tickStart + viewport_.tickSpan;

    for (const auto& n : pool) {
        if (n.on >= vpEnd) break;
        if (n.on + n.duration < viewport_.tickStart) continue;

        int16_t lane = int16_t(n.pitch) - int16_t(viewport_.pitchBase);
        if (lane < 0 || lane >= int16_t(lanes)) continue;

        int32_t x0 = xFromTick(n.on);
        int32_t x1 = xFromTick(n.on + n.duration);

        if (x0 < gx0) x0 = gx0;
        if (x1 > gx1) x1 = gx1;
        if (x1 <= x0) continue;

        int16_t  y = yFromPitch(n.pitch) + (LANE_H - NOTE_H) / 2;  // centre note bar in lane
        uint16_t w = uint16_t(x1 - x0);
        if (w < 2) w = 2;

        g.fillRect(x0, y, w, NOTE_H, velToGray(n.vel));   // body brightness = velocity
        // Leading 2×NOTE_H block always full bright — marks the note onset clearly.
        g.fillRect(x0, y, w < 4 ? w : 4, NOTE_H, 15);
    }
}

void PianoRollView::drawPlayhead(GrayCanvas& g, const UICtx& ctx) {
    if (!ctx.transport.isRunning()) return;
    int32_t x = xFromTick(ctx.transport.playTick());
    if (x < LABEL_W || x >= LABEL_W + GRID_W) return;
    for (int16_t y = GRID_Y; y < GRID_Y + GRID_H; ++y)
        g.setPixel(x, y, 15);
}

// Mark the focused track's end: a dim boundary line at trackLen; buckets past it
// stay black (non-interactive). Anything left of the window edge is ignored.
void PianoRollView::drawTrackEnd(GrayCanvas& g, const UICtx& ctx) {
    const uint32_t len = ctx.pat.tracks[ctx.sequencer.getTrack()].lenTicks;
    int32_t x = xFromTick(len);
    if (x <= LABEL_W || x > LABEL_W + GRID_W) return;   // end not in view
    for (int16_t y = GRID_Y; y < GRID_Y + GRID_H; y += 2)
        g.setPixel(x, y, 9);   // dashed dim boundary
}

// Edit cursor: a dim dashed column at cursorTick + a bright caret on its pitch lane.
void PianoRollView::drawCursor(GrayCanvas& g, const UICtx& ctx) {
    const uint32_t cur = ctx.sequencer.getCursorTick();
    const uint32_t gridT = timebase::ticksPerStep(ctx.pat.grid);
    int32_t x0 = xFromTick(cur);
    int32_t x1 = xFromTick(cur + gridT);
    if (x1 <= LABEL_W || x0 >= LABEL_W + GRID_W) return;
    if (x0 < LABEL_W) x0 = LABEL_W;
    if (x1 > LABEL_W + GRID_W) x1 = LABEL_W + GRID_W;

    // Dim dashed column (every 2px) so it reads differently from the bright playhead.
    for (int16_t y = GRID_Y; y < GRID_Y + GRID_H; y += 2)
        g.setPixel(x0, y, 6);

    // Bright caret on the cursor's target pitch lane (where new notes land).
    int16_t ly = yFromPitch(ctx.sequencer.getEditPitch()) + (LANE_H - NOTE_H) / 2;
    g.fillRect(x0, ly, (x1 - x0) < 3 ? (x1 - x0) : 3, NOTE_H, 13);
}

// ── Main draw entry point ──────────────────────────────────────────────────

void PianoRollView::draw(OledRenderer& oled, const UICtx& ctx) {
    U8G2&       gfx = oled.gfx();    // 1-bit layer: header text, piano-key labels
    GrayCanvas& g   = oled.gray();   // gray layer: grid, notes, playhead

    // E1 moves the cursor = window left edge (pad 0); the 16 buckets span the width.
    viewport_.tickStart = ctx.sequencer.getWindowStart();
    viewport_.tickSpan  = seq::visibleSpanTicks(ctx.pat.grid);

    // E2 (cursor pitch lane) → up/down scroll: centre the cursor lane in visible lanes
    const int16_t half = int16_t(numLanes()) / 2;
    int16_t base = int16_t(ctx.sequencer.getEditPitch()) - half;
    if (base < 0)   base = 0;
    if (base > 119) base = 119;
    viewport_.pitchBase = uint8_t(base);

    const auto& track = ctx.pat.tracks[ctx.sequencer.getTrack()];

    // Gray layer (bottom→top): grid behind notes, playhead + cursor on top.
    drawGrid     (g, ctx);
    drawNotePool (g, track.recorded);
    drawNotePool (g, track.generative);
    drawTrackEnd (g, ctx);   // mark + darken buckets past the track length
    drawPlayhead (g, ctx);
    drawCursor   (g, ctx);

    // 1-bit layer — composited over the gray layer at full brightness in send().
    drawPianoKeys(gfx, ctx);
    drawHeader   (gfx, ctx);
}
