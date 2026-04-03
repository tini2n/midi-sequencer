#pragma once
#include <Arduino.h>
#include "matrix_kb_mode.hpp"
#include "../model/note.hpp"
#include "../model/pattern.hpp"

// Digitakt-style step sequencer editing mode.
//
// The 16 keyboard buttons map to 16 step slots on the active track.
// Pressing a button toggles the note at that step on/off.
// Page offset (set by encoder 0) shifts which 16 steps are visible:
//   actual step = btn + pageOffset * 16
// Edit pitch (set by encoder 1) is the MIDI note assigned to new steps.
//
// After every note change, the track state is printed to Serial as an ASCII grid.
//
// Control buttons (CTL row):
//   CTL 0 — toggle active track (0/1)
//   CTL 1 — play/pause (via RunLoop)
//   CTL 2 — stop (via RunLoop)
//   CTL 7 — shift modifier (held)
//   Shift + CTL 0 — clear step
//   Shift + CTL 1 — copy step
//   Shift + CTL 2 — paste step
class CursorMode : public IMatrixKBMode {
public:
    void onButtonDown(uint8_t btn, MidiIO& midi, uint8_t ch, void* context) override;
    void onButtonUp(uint8_t btn, MidiIO& midi, uint8_t ch, void* context) override;
    void update(uint32_t now, void* context) override;
    void onActivate() override;
    void onDeactivate() override;
    bool onControl(uint8_t c, bool down, bool shift, void* context) override;

    void configure(const IModeConfig& cfg) override { editPitch_ = cfg.editPitch; }
    void getConfig(IModeConfig& cfg) const override  { cfg.editPitch = editPitch_; }

    void setEditPitch(uint8_t pitch);
    void setPage(uint8_t page, uint8_t patternSteps = 255);
    void setTrack(uint8_t t) { trackIdx_ = (t < MAX_TRACKS) ? t : 0; }

    uint8_t getEditPitch()   const { return editPitch_; }
    uint8_t getPage()        const { return pageOffset_; }
    uint8_t getTrack()       const { return trackIdx_; }
    bool    isShiftPressed() const { return shiftPressed_; }
    int8_t  getHeldStep()    const { return heldStep_; }

    // Edit a property of the currently held step's note.
    // param: 0=pitch, 1=velocity, 2=micro-offset (tick nudge).
    // Safe to call even when no step is held (no-op).
    void editHeld(uint8_t param, int8_t delta, Pattern& pat);

    // Print the current track state as an ASCII step grid to Serial.
    void printTrackState(const Pattern& pat) const;

    // Step editing operations (also callable from SerialMonitor).
    void toggleStep(uint8_t step, Pattern& pat);
    void copyStep(Pattern& pat);
    void pasteToStep(Pattern& pat);
    void clearStep(Pattern& pat);

private:
    uint8_t pageOffset_{0};  // which page of 16 steps is on the buttons
    uint8_t editPitch_{60};  // C4 — MIDI note for new trigs
    uint8_t trackIdx_{0};    // which track is being edited (0 or 1)
    uint8_t selectedStep_{0};
    bool    shiftPressed_{false};

    int8_t  heldStep_{-1};         // step button currently held (-1 = none)
    bool    editedWhileHeld_{false}; // encoder moved during hold — suppress re-toggle on up

    Note    copyBuffer_{};
    bool    hasCopy_{false};

    // Returns pointer to note at step tick in the recorded pool, or nullptr.
    Note*    findNoteAtStep(uint8_t step, Pattern& pat);
    uint32_t stepToTick(uint8_t step, const Pattern& pat) const;

    static const char* pitchName(uint8_t pitch, char* buf, uint8_t bufLen);
};
