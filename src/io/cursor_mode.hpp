#pragma once
#include "matrix_kb_mode.hpp"
#include "model/note.hpp"
#include "model/pattern.hpp"
#include <Arduino.h>

/**
 * Cursor/Editor mode for matrix keyboard in GenerativeView.
 * Treats 16 buttons as step selectors in a sequencer (Digitakt-style).
 * 
 * Button Layout (2×8):
 *   Top:    [0][1][2][3][4][5][6][7]     → Steps 1-8
 *   Bottom: [8][9][10][11][12][13][14][15] → Steps 9-16
 * 
 * Actions:
 * - Press button → Select step + toggle note at current edit pitch
 * - Shift + Rec (Ctl0) → Clear selected step
 * - Shift + Play (Ctl1) → Copy selected step
 * - Shift + Stop (Ctl2) → Paste to selected step
 */
class CursorMode : public IMatrixKBMode
{
public:
    void onButtonDown(uint8_t btn, MidiIO &midi, uint8_t ch, void *context) override;
    void onButtonUp(uint8_t btn, MidiIO &midi, uint8_t ch, void *context) override;
    void update(uint32_t now, void *context) override;
    void onActivate() override;
    void onDeactivate() override;

    // Cursor mode specific API
    void setEditPitch(uint8_t pitch);
    void setShiftPressed(bool pressed) { shiftPressed_ = pressed; }
    uint8_t getSelectedStep() const { return selectedStep_; }
    uint8_t getEditPitch() const { return editPitch_; }

    // Edit operations (called by MatrixKB control row)
    void copyStep(Pattern &pattern);
    void pasteToStep(Pattern &pattern);
    void clearStep(Pattern &pattern);

private:
    uint8_t selectedStep_{0};    // Current cursor position (0-15)
    uint8_t editPitch_{60};      // Current edit pitch (C4 default)
    bool shiftPressed_{false};   // Shift button state (Ctl7)
    
    // Copy buffer
    Note copyBuffer_;
    bool hasCopy_{false};

    // Action handlers
    void selectStepAndToggle(uint8_t step, Pattern &pattern);

    // Helpers
    Note *findNoteAtStep(uint8_t step, Pattern &pattern);
    uint32_t stepToTick(uint8_t step, const Pattern &pattern) const;
};
