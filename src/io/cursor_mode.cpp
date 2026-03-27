#include "cursor_mode.hpp"
#include "../types.hpp"

// ─── IMatrixKBMode interface ─────────────────────────────────────────────────

void CursorMode::onButtonDown(uint8_t btn, MidiIO& midi, uint8_t ch, void* context) {
    (void)midi; (void)ch;
    if (!context || btn >= 16) return;
    Pattern* pat = static_cast<Pattern*>(context);
    uint8_t actualStep = btn + pageOffset_ * 16;
    selectedStep_ = actualStep;
    toggleStep(actualStep, *pat);
}

void CursorMode::onButtonUp(uint8_t btn, MidiIO& midi, uint8_t ch, void* context) {
    (void)btn; (void)midi; (void)ch; (void)context;
}

void CursorMode::update(uint32_t now, void* context) {
    (void)now; (void)context;
}

void CursorMode::onActivate() {
    Serial.println("[CursorMode] active — 16 buttons = 16 steps");
    Serial.println("  press = toggle note  |  shift+CTL0 = clear  |  shift+CTL1 = copy  |  shift+CTL2 = paste");
}

void CursorMode::onDeactivate() {}

bool CursorMode::onControl(uint8_t c, bool down, bool shift, void* context) {
    if (c == 7) { shiftPressed_ = down; return true; }
    if (!down) return false;

    if (shift && context) {
        Pattern& pat = *static_cast<Pattern*>(context);
        switch (c) {
        case 0: clearStep(pat); return true;
        case 1: copyStep(pat);  return true;
        case 2: pasteToStep(pat); return true;
        }
    }
    return false; // let MatrixKB handle CTL 0/1/2 as transport / track switch
}

// ─── Pitch / page controls ────────────────────────────────────────────────────

void CursorMode::setEditPitch(uint8_t pitch) {
    if (pitch > 127) pitch = 127;
    editPitch_ = pitch;
    char buf[5];
    Serial.printf("[Cursor] pitch -> %s (%u)\n", pitchName(pitch, buf, sizeof(buf)), pitch);
}

void CursorMode::setPage(uint8_t page, uint8_t patternSteps) {
    uint8_t maxPage = (patternSteps > 16) ? ((patternSteps - 1) / 16) : 0;
    if (page > maxPage) page = maxPage;
    pageOffset_ = page;
    Serial.printf("[Cursor] page -> %u (steps %u-%u)\n",
                  page, page * 16, page * 16 + 15);
}

// ─── Step editing ─────────────────────────────────────────────────────────────

void CursorMode::toggleStep(uint8_t step, Pattern& pat) {
    Note* existing = findNoteAtStep(step, pat);
    NotePool<256>& pool = pat.tracks[trackIdx_].recorded;

    if (existing) {
        // Remove: find its index in the pool
        for (uint16_t i = 0; i < pool.count; ++i) {
            if (&pool.notes[i] == existing) {
                pool.removeAt(i);
                break;
            }
        }
        char buf[5];
        Serial.printf("[Cursor] trk%u step%u OFF (%s removed)\n",
                      trackIdx_, step, pitchName(existing->pitch, buf, sizeof(buf)));
    } else {
        Note n{};
        n.on       = stepToTick(step, pat);
        n.duration = pat.ticks() / pat.steps; // one step long
        n.pitch    = editPitch_;
        n.vel      = 100;
        n.micro_q8 = 0;
        n.flags    = 0;
        if (!pool.push(n)) {
            Serial.println("[Cursor] pool full — note not added");
            return;
        }
        pool.sortByOnTick();
        char buf[5];
        Serial.printf("[Cursor] trk%u step%u ON  (%s added)\n",
                      trackIdx_, step, pitchName(n.pitch, buf, sizeof(buf)));
    }
    printTrackState(pat);
}

void CursorMode::copyStep(Pattern& pat) {
    Note* n = findNoteAtStep(selectedStep_, pat);
    if (n) {
        copyBuffer_ = *n;
        hasCopy_    = true;
        char buf[5];
        Serial.printf("[Cursor] copied step%u (%s)\n",
                      selectedStep_, pitchName(n->pitch, buf, sizeof(buf)));
    } else {
        Serial.printf("[Cursor] step%u empty — nothing to copy\n", selectedStep_);
    }
}

void CursorMode::pasteToStep(Pattern& pat) {
    if (!hasCopy_) { Serial.println("[Cursor] copy buffer empty"); return; }
    NotePool<256>& pool = pat.tracks[trackIdx_].recorded;
    uint32_t tick = stepToTick(selectedStep_, pat);

    // Clear any existing note at this tick
    for (uint16_t i = 0; i < pool.count; ) {
        if (pool.notes[i].on == tick) pool.removeAt(i);
        else ++i;
    }
    Note pasted    = copyBuffer_;
    pasted.on      = tick;
    pasted.duration = pat.ticks() / pat.steps;
    if (pool.push(pasted)) {
        pool.sortByOnTick();
        char buf[5];
        Serial.printf("[Cursor] pasted step%u (%s)\n",
                      selectedStep_, pitchName(pasted.pitch, buf, sizeof(buf)));
        printTrackState(pat);
    }
}

void CursorMode::clearStep(Pattern& pat) {
    NotePool<256>& pool = pat.tracks[trackIdx_].recorded;
    uint32_t tick = stepToTick(selectedStep_, pat);
    uint16_t before = pool.count;
    for (uint16_t i = 0; i < pool.count; ) {
        if (pool.notes[i].on == tick) pool.removeAt(i);
        else ++i;
    }
    if (pool.count < before) {
        Serial.printf("[Cursor] cleared step%u\n", selectedStep_);
        printTrackState(pat);
    } else {
        Serial.printf("[Cursor] step%u already empty\n", selectedStep_);
    }
}

// ─── Serial state display ─────────────────────────────────────────────────────

void CursorMode::printTrackState(const Pattern& pat) const {
    const NotePool<256>& pool = pat.tracks[trackIdx_].recorded;
    const uint8_t  ch    = pat.tracks[trackIdx_].channel;
    const uint8_t  start = pageOffset_ * 16;
    const uint8_t  end   = start + 16; // exclusive

    char pitchBuf[5];
    Serial.printf("Trk%u ch%u  page%u  pitch:%s  notes:%u\n",
                  trackIdx_, ch, pageOffset_,
                  pitchName(editPitch_, pitchBuf, sizeof(pitchBuf)),
                  pool.count);

    // Row 1: step active markers
    for (uint8_t s = start; s < end; ++s) {
        uint32_t tick = stepToTick(s, pat);
        bool active = false;
        for (uint16_t i = 0; i < pool.count; ++i)
            if (pool.notes[i].on == tick) { active = true; break; }
        Serial.print(active ? "[X]" : "[ ]");
    }
    Serial.println();

    // Row 2: pitch labels (only where note exists)
    for (uint8_t s = start; s < end; ++s) {
        uint32_t tick = stepToTick(s, pat);
        char label[4] = "   ";
        for (uint16_t i = 0; i < pool.count; ++i) {
            if (pool.notes[i].on == tick) {
                pitchName(pool.notes[i].pitch, pitchBuf, sizeof(pitchBuf));
                // Left-pad to 3 chars
                uint8_t len = 0;
                while (pitchBuf[len]) len++;
                if (len >= 3) { label[0]=pitchBuf[0]; label[1]=pitchBuf[1]; label[2]=pitchBuf[2]; }
                else if (len == 2) { label[0]=' '; label[1]=pitchBuf[0]; label[2]=pitchBuf[1]; }
                else { label[0]=' '; label[1]=' '; label[2]=pitchBuf[0]; }
                break;
            }
        }
        Serial.write((uint8_t*)label, 3);
    }
    Serial.println();
}

// ─── Helpers ──────────────────────────────────────────────────────────────────

Note* CursorMode::findNoteAtStep(uint8_t step, Pattern& pat) {
    NotePool<256>& pool = pat.tracks[trackIdx_].recorded;
    uint32_t tick = stepToTick(step, pat);
    for (uint16_t i = 0; i < pool.count; ++i)
        if (pool.notes[i].on == tick) return &pool.notes[i];
    return nullptr;
}

uint32_t CursorMode::stepToTick(uint8_t step, const Pattern& pat) const {
    if (pat.steps == 0) return 0;
    uint8_t clamped = (step >= pat.steps) ? (pat.steps - 1) : step;
    return uint32_t(clamped) * (pat.ticks() / pat.steps);
}

const char* CursorMode::pitchName(uint8_t pitch, char* buf, uint8_t bufLen) {
    static const char* names[12] = {"C","C#","D","D#","E","F","F#","G","G#","A","A#","B"};
    if (bufLen < 5) { buf[0] = '\0'; return buf; }
    int8_t oct = (pitch / 12) - 1;
    snprintf(buf, bufLen, "%s%d", names[pitch % 12], oct);
    return buf;
}
