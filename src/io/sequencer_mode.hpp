#pragma once
#include <Arduino.h>
#include "keyboard_mode.hpp"
#include "../model/note.hpp"
#include "../model/pattern.hpp"
#include "../types.hpp"

// Digitakt-style step sequencer editing mode (tick-based, grid-driven).
//
// The 16 keyboard buttons map 1:1 to the 16 time buckets currently on screen:
//   bucket i covers tick = windowStartTick + i * gridTicks(grid).
// Pressing a button toggles the note in that bucket and moves the cursor onto it.
//
// Navigation is cursor-based, not page-based:
//   E1            moves the cursor ±1 grid cell (Shift = ±16); the window follows.
//   E4 (grid)     sets the resolution = zoom = default note length (Pattern::grid).
//   E5            sets the focused track length in grid increments (Shift = ±16).
//
// All note positions/lengths are absolute ticks; GRID never rewrites stored data,
// it only drives the view and the new-note default length (one bucket).
//
// Control buttons (CTL row):
//   CTL 0 — toggle active track
//   CTL 1 — play/pause (via RunLoop)
//   CTL 2 — stop (via RunLoop)
//   CTL 7 — shift modifier (held) → ±16 increments on E1/E5
//   Shift + CTL 0 — clear bucket at cursor
//   Shift + CTL 1 — copy bucket at cursor
//   Shift + CTL 2 — paste bucket at cursor
class SequencerMode : public IKeyboardMode {
public:
    void onButtonDown(uint8_t btn, MidiIO& midi, uint8_t ch, void* context) override;
    void onButtonUp(uint8_t btn, MidiIO& midi, uint8_t ch, void* context) override;
    void update(uint32_t now, void* context) override;
    void onActivate() override;
    void onDeactivate() override;
    bool onControl(uint8_t c, bool down, bool shift, void* context) override;

    void configure(const IModeConfig& cfg) override { editPitch_ = cfg.editPitch; }
    void getConfig(IModeConfig& cfg) const override  { cfg.editPitch = editPitch_; }

    // ── Edit parameters ─────────────────────────────────────────────────────
    void setEditPitch(uint8_t pitch);
    void setEditVelocity(uint8_t vel);
    void setTrack(uint8_t t);

    uint8_t  getEditPitch()    const { return editPitch_; }
    uint8_t  getEditVelocity() const { return editVelocity_; }
    uint8_t  getTrack()        const { return trackIdx_; }
    bool     isShiftPressed()  const { return shiftPressed_; }
    int32_t  getHeldTick()     const { return heldTick_; }
    uint32_t getCursorTick()   const { return cursorTick_; }
    uint32_t getWindowStart()  const { return cursorTick_; }  // cursor = left edge / pad 0

    // ── Navigation / zoom / length (driven by encoders) ─────────────────────
    void moveCursor(int deltaTicks, Pattern& pat);     // E1
    void onGridChanged(Pattern& pat);                  // after E4 changes Pattern::grid
    void editTrackLen(int deltaTicks, Pattern& pat);   // E5

    // Edit a property of the note under the cursor (when a bucket is held).
    // param: 0=pitch  1=velocity  2=tick nudge  3=duration (grid cells)
    void editHeld(uint8_t param, int8_t delta, Pattern& pat);

    // Print the visible window of the focused track as an ASCII grid to Serial.
    void printTrackState(const Pattern& pat) const;

    // Bucket editing operations (also callable from SerialMonitor); act at cursor.
    void toggleAtTick(uint32_t tick, Pattern& pat);
    void copyAtCursor(Pattern& pat);
    void pasteAtCursor(Pattern& pat);
    void clearAtCursor(Pattern& pat);

private:
    // The cursor is the left edge of the 16-pad row: pad i edits cursorTick_ + i*gridTicks.
    // Moving the cursor (E1) remaps all pads; pressing a pad never moves the cursor.
    uint32_t cursorTick_{0};       // anchor / window left edge, absolute ticks (grid-aligned)
    uint8_t  editPitch_{60};       // C5 — cursor pitch lane / new-note pitch
    uint8_t  editVelocity_{100};   // velocity for new notes
    uint8_t  trackIdx_{0};         // focused track
    bool     shiftPressed_{false};

    int32_t  heldTick_{-1};         // bucket tick currently held (-1 = none)
    uint32_t heldStartMs_{0};       // millis() at press, for quick-tap detection
    bool     editedWhileHeld_{false};
    bool     addedOnPress_{false};  // note was created on this press (empty bucket)
    static constexpr uint32_t kTapMs = 500;  // quick-tap threshold; longer = hold-to-edit

    Note     copyBuffer_{};
    bool     hasCopy_{false};

    // Largest in-track bucket start for the focused track at the current grid.
    uint32_t lastBucketStart(const Pattern& pat) const;

    Note*    findNoteAtTick(uint32_t tick, Pattern& pat);
    void     addNoteAtTick(uint32_t tick, Pattern& pat);
    void     removeNoteAtTick(uint32_t tick, Pattern& pat);

    static const char* pitchName(uint8_t pitch, char* buf, uint8_t bufLen);
};
