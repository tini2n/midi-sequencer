#include "cursor_mode.hpp"
#include "core/timebase.hpp"
#include <algorithm>

void CursorMode::onButtonDown(uint8_t btn, MidiIO &midi, uint8_t ch, void *context)
{
    (void)midi;
    (void)ch;
    
    if (!context)
        return;
    Pattern *pattern = static_cast<Pattern *>(context);

    // Button 0-15: Select step and toggle note
    if (btn < 16)
    {
        selectedStep_ = btn;
        selectStepAndToggle(btn, *pattern);
    }
}

void CursorMode::onButtonUp(uint8_t btn, MidiIO &midi, uint8_t ch, void *context)
{
    (void)btn;
    (void)midi;
    (void)ch;
    (void)context;
    // No action on release in cursor mode
}

void CursorMode::update(uint32_t now, void *context)
{
    (void)now;
    (void)context;
    // Future: gesture detection, hold timing, etc.
}

void CursorMode::onActivate()
{
    Serial.println("[CursorMode] Activated");
    Serial.println("  Press button → Select step + toggle note");
    Serial.println("  Shift + Rec → Clear step");
    Serial.println("  Shift + Play → Copy step");
    Serial.println("  Shift + Stop → Paste step");
}

void CursorMode::onDeactivate()
{
    Serial.println("[CursorMode] Deactivated");
}

void CursorMode::setEditPitch(uint8_t pitch)
{
    if (pitch > 127)
        pitch = 127;
    editPitch_ = pitch;
    Serial.printf("[CursorMode] Edit pitch set to %u\n", pitch);
}

void CursorMode::selectStepAndToggle(uint8_t step, Pattern &pattern)
{
    selectedStep_ = step;
    uint32_t tick = stepToTick(step, pattern);
    Note *existing = findNoteAtStep(step, pattern);

    if (existing)
    {
        // Remove note
        pattern.track.notes.erase(
            std::remove_if(pattern.track.notes.begin(),
                           pattern.track.notes.end(),
                           [tick](const Note &n)
                           { return n.on == tick; }),
            pattern.track.notes.end());
        Serial.printf("[CursorMode] Removed note at step %u (tick %u)\n", step, tick);
    }
    else
    {
        // Add note at current edit pitch
        Note newNote;
        newNote.on = tick;
        newNote.duration = pattern.ticks() / pattern.steps; // One step duration
        newNote.pitch = editPitch_;
        newNote.vel = 100;
        newNote.micro = 0;
        newNote.micro_q8 = 0;
        newNote.flags = 0;
        pattern.track.notes.push_back(newNote);
        Serial.printf("[CursorMode] Added note %u at step %u (tick %u)\n",
                      editPitch_, step, tick);
    }
}

void CursorMode::copyStep(Pattern &pattern)
{
    Note *note = findNoteAtStep(selectedStep_, pattern);
    if (note)
    {
        copyBuffer_ = *note;
        hasCopy_ = true;
        Serial.printf("[CursorMode] Copied step %u (pitch %u)\n", selectedStep_, note->pitch);
    }
    else
    {
        Serial.printf("[CursorMode] Nothing to copy at step %u\n", selectedStep_);
    }
}

void CursorMode::pasteToStep(Pattern &pattern)
{
    if (!hasCopy_)
    {
        Serial.println("[CursorMode] Copy buffer empty");
        return;
    }

    // Clear existing note at target step
    uint32_t tick = stepToTick(selectedStep_, pattern);
    pattern.track.notes.erase(
        std::remove_if(pattern.track.notes.begin(),
                       pattern.track.notes.end(),
                       [tick](const Note &n)
                       { return n.on == tick; }),
        pattern.track.notes.end());

    // Paste copied note at new position
    Note newNote = copyBuffer_;
    newNote.on = tick;
    pattern.track.notes.push_back(newNote);
    Serial.printf("[CursorMode] Pasted to step %u (pitch %u)\n", selectedStep_, newNote.pitch);
}

void CursorMode::clearStep(Pattern &pattern)
{
    uint32_t tick = stepToTick(selectedStep_, pattern);
    size_t before = pattern.track.notes.size();
    pattern.track.notes.erase(
        std::remove_if(pattern.track.notes.begin(),
                       pattern.track.notes.end(),
                       [tick](const Note &n)
                       { return n.on == tick; }),
        pattern.track.notes.end());
    size_t after = pattern.track.notes.size();
    
    if (before != after)
        Serial.printf("[CursorMode] Cleared step %u\n", selectedStep_);
    else
        Serial.printf("[CursorMode] Step %u already empty\n", selectedStep_);
}

Note *CursorMode::findNoteAtStep(uint8_t step, Pattern &pattern)
{
    uint32_t tick = stepToTick(step, pattern);
    for (auto &note : pattern.track.notes)
    {
        if (note.on == tick)
            return &note;
    }
    return nullptr;
}

uint32_t CursorMode::stepToTick(uint8_t step, const Pattern &pattern) const
{
    uint32_t ticksPerStep = pattern.ticks() / pattern.steps;
    return step * ticksPerStep;
}
