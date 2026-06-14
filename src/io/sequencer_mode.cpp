#include "sequencer_mode.hpp"
#include "../types.hpp"

// ─── IKeyboardMode interface ─────────────────────────────────────────────────

void SequencerMode::onButtonDown(uint8_t btn, MidiIO& midi, uint8_t ch, void* context) {
    (void)midi; (void)ch;
    if (!context || btn >= 16) return;
    Pattern* pat = static_cast<Pattern*>(context);
    uint8_t actualStep = btn + pageOffset_ * 16;
    selectedStep_    = actualStep;
    heldStep_        = (int8_t)actualStep;
    editedWhileHeld_ = false;
    toggleStep(actualStep, *pat);
}

void SequencerMode::onButtonUp(uint8_t btn, MidiIO& midi, uint8_t ch, void* context) {
    (void)midi; (void)ch; (void)context;
    if (btn >= 16) return;
    heldStep_        = -1;
    editedWhileHeld_ = false;
}

void SequencerMode::update(uint32_t now, void* context) {
    (void)now; (void)context;
}

void SequencerMode::onActivate() {
    Serial.println("[SequencerMode] active");
}

void SequencerMode::onDeactivate() {}

bool SequencerMode::onControl(uint8_t c, bool down, bool shift, void* context) {
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
    return false;
}

// ─── Pitch / page controls ────────────────────────────────────────────────────

void SequencerMode::setEditPitch(uint8_t pitch) {
    if (pitch > 127) pitch = 127;
    editPitch_ = pitch;
    char buf[5];
    Serial.printf("[Sequencer] pitch -> %s\n", pitchName(pitch, buf, sizeof(buf)));
}

void SequencerMode::setEditVelocity(uint8_t vel) {
    if (vel < 1)   vel = 1;
    if (vel > 127) vel = 127;
    editVelocity_ = vel;
    Serial.printf("[Sequencer] vel -> %u\n", vel);
}

void SequencerMode::setEditLength(uint8_t steps) {
    if (steps < 1) steps = 1;
    editLength_ = steps;
    Serial.printf("[Sequencer] len -> %u\n", steps);
}

void SequencerMode::setPage(uint8_t page, uint8_t patternSteps) {
    uint8_t maxPage = (patternSteps > 16) ? ((patternSteps - 1) / 16) : 0;
    if (page > maxPage) page = maxPage;
    pageOffset_ = page;
    Serial.printf("[Sequencer] page -> %u\n", page);
}

// ─── Step editing ─────────────────────────────────────────────────────────────

void SequencerMode::toggleStep(uint8_t step, Pattern& pat) {
    Note* existing = findNoteAtStep(step, pat);
    NotePool<128>& pool = pat.tracks[trackIdx_].recorded;

    if (existing) {
        for (uint16_t i = 0; i < pool.count; ++i) {
            if (&pool.notes[i] == existing) { pool.removeAt(i); break; }
        }
        char buf[5];
        Serial.printf("[Sequencer] trk%u step%u OFF (%s removed)\n",
                      trackIdx_, step, pitchName(existing->pitch, buf, sizeof(buf)));
    } else {
        Note n{};
        n.on       = stepToTick(step, pat);
        n.duration = (uint32_t)editLength_ * (pat.ticks() / pat.steps);
        n.pitch    = editPitch_;
        n.vel      = editVelocity_;
        n.micro_q8 = 0;
        n.flags    = 0;
        if (!pool.push(n)) {
            Serial.println("[Sequencer] pool full");
            return;
        }
        pool.sortByOnTick();
        char buf[5];
        Serial.printf("[Sequencer] trk%u step%u ON  (%s added)\n",
                      trackIdx_, step, pitchName(n.pitch, buf, sizeof(buf)));
    }
}

void SequencerMode::copyStep(Pattern& pat) {
    Note* n = findNoteAtStep(selectedStep_, pat);
    if (n) {
        copyBuffer_ = *n;
        hasCopy_    = true;
        char buf[5];
        Serial.printf("[Sequencer] copied step%u (%s)\n",
                      selectedStep_, pitchName(n->pitch, buf, sizeof(buf)));
    } else {
        Serial.printf("[Sequencer] step%u empty\n", selectedStep_);
    }
}

void SequencerMode::pasteToStep(Pattern& pat) {
    if (!hasCopy_) { Serial.println("[Sequencer] copy buffer empty"); return; }
    NotePool<128>& pool = pat.tracks[trackIdx_].recorded;
    uint32_t tick = stepToTick(selectedStep_, pat);
    for (uint16_t i = 0; i < pool.count; ) {
        if (pool.notes[i].on == tick) pool.removeAt(i);
        else ++i;
    }
    Note pasted     = copyBuffer_;
    pasted.on       = tick;
    pasted.duration = pat.ticks() / pat.steps;
    if (pool.push(pasted)) {
        pool.sortByOnTick();
        char buf[5];
        Serial.printf("[Sequencer] pasted step%u (%s)\n",
                      selectedStep_, pitchName(pasted.pitch, buf, sizeof(buf)));
    }
}

void SequencerMode::clearStep(Pattern& pat) {
    NotePool<128>& pool = pat.tracks[trackIdx_].recorded;
    uint32_t tick = stepToTick(selectedStep_, pat);
    uint16_t before = pool.count;
    for (uint16_t i = 0; i < pool.count; ) {
        if (pool.notes[i].on == tick) pool.removeAt(i);
        else ++i;
    }
    if (pool.count < before)
        Serial.printf("[Sequencer] cleared step%u\n", selectedStep_);
    else
        Serial.printf("[Sequencer] step%u empty\n", selectedStep_);
}

// ─── Serial state display ─────────────────────────────────────────────────────

void SequencerMode::printTrackState(const Pattern& pat) const {
    const NotePool<128>& pool = pat.tracks[trackIdx_].recorded;
    const uint8_t  ch    = pat.tracks[trackIdx_].channel;
    const uint8_t  start = pageOffset_ * 16;
    const uint8_t  end   = start + 16;

    char pitchBuf[5];
    Serial.printf("Trk%u ch%u  page%u  pitch:%s  vel:%u  len:%u  notes:%u\n",
                  trackIdx_, ch, pageOffset_,
                  pitchName(editPitch_, pitchBuf, sizeof(pitchBuf)),
                  editVelocity_, editLength_, pool.count);

    for (uint8_t s = start; s < end; ++s) {
        uint32_t tick = stepToTick(s, pat);
        bool active = false;
        for (uint16_t i = 0; i < pool.count; ++i)
            if (pool.notes[i].on == tick) { active = true; break; }
        Serial.print(active ? "[X]" : "[ ]");
    }
    Serial.println();

    for (uint8_t s = start; s < end; ++s) {
        uint32_t tick = stepToTick(s, pat);
        char label[4] = "   ";
        for (uint16_t i = 0; i < pool.count; ++i) {
            if (pool.notes[i].on == tick) {
                pitchName(pool.notes[i].pitch, pitchBuf, sizeof(pitchBuf));
                uint8_t len = 0;
                while (pitchBuf[len]) len++;
                if (len >= 3)      { label[0]=pitchBuf[0]; label[1]=pitchBuf[1]; label[2]=pitchBuf[2]; }
                else if (len == 2) { label[0]=' '; label[1]=pitchBuf[0]; label[2]=pitchBuf[1]; }
                else               { label[0]=' '; label[1]=' '; label[2]=pitchBuf[0]; }
                break;
            }
        }
        Serial.write((uint8_t*)label, 3);
    }
    Serial.println();
}

// ─── Helpers ──────────────────────────────────────────────────────────────────

Note* SequencerMode::findNoteAtStep(uint8_t step, Pattern& pat) {
    NotePool<128>& pool = pat.tracks[trackIdx_].recorded;
    uint32_t tick = stepToTick(step, pat);
    for (uint16_t i = 0; i < pool.count; ++i)
        if (pool.notes[i].on == tick) return &pool.notes[i];
    return nullptr;
}

uint32_t SequencerMode::stepToTick(uint8_t step, const Pattern& pat) const {
    if (pat.steps == 0) return 0;
    uint8_t clamped = (step >= pat.steps) ? (pat.steps - 1) : step;
    return uint32_t(clamped) * (pat.ticks() / pat.steps);
}

void SequencerMode::editHeld(uint8_t param, int8_t delta, Pattern& pat) {
    if (heldStep_ < 0 || delta == 0) return;
    Note* n = findNoteAtStep((uint8_t)heldStep_, pat);
    if (!n) return;

    editedWhileHeld_ = true;
    NotePool<128>& pool = pat.tracks[trackIdx_].recorded;

    switch (param) {
    case 0: { // pitch
        int16_t p = (int16_t)n->pitch + delta;
        n->pitch = (uint8_t)(p < 0 ? 0 : (p > 127 ? 127 : p));
        char buf[5];
        Serial.printf("[Hold] step%d pitch -> %s\n", heldStep_, pitchName(n->pitch, buf, sizeof(buf)));
        break;
    }
    case 1: { // velocity
        int16_t v = (int16_t)n->vel + delta;
        n->vel = (uint8_t)(v < 1 ? 1 : (v > 127 ? 127 : v));
        Serial.printf("[Hold] step%d vel -> %u\n", heldStep_, n->vel);
        break;
    }
    case 2: { // tick nudge
        int32_t t = (int32_t)n->on + delta;
        n->on = (uint32_t)(t < 0 ? 0 : t);
        pool.sortByOnTick();
        Serial.printf("[Hold] step%d tick -> %lu\n", heldStep_, (unsigned long)n->on);
        break;
    }
    case 3: { // duration in steps
        uint32_t tps = pat.ticks() / pat.steps;
        if (tps == 0) break;
        int32_t cur  = (int32_t)(n->duration / tps);
        int32_t next = cur + delta;
        if (next < 1)   next = 1;
        if (next > 128) next = 128;
        n->duration = (uint32_t)next * tps;
        Serial.printf("[Hold] step%d dur -> %d\n", heldStep_, (int)next);
        break;
    }
    default: break;
    }
}

const char* SequencerMode::pitchName(uint8_t pitch, char* buf, uint8_t bufLen) {
    static const char* names[12] = {"C","C#","D","D#","E","F","F#","G","G#","A","A#","B"};
    if (bufLen < 5) { buf[0] = '\0'; return buf; }
    int8_t oct = pitch / 12;   // note 0 = C0 (matches the piano-roll display)
    snprintf(buf, bufLen, "%s%d", names[pitch % 12], oct);
    return buf;
}
