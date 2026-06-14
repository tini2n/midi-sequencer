#include "sequencer_mode.hpp"
#include "../types.hpp"

// ─── IKeyboardMode interface ─────────────────────────────────────────────────

void SequencerMode::onButtonDown(uint8_t btn, MidiIO& midi, uint8_t ch, void* context) {
    (void)midi; (void)ch;
    if (!context || btn >= seq::BUCKETS) return;
    Pattern& pat = *static_cast<Pattern*>(context);

    const uint32_t g    = seq::gridTicks(pat.grid);
    const uint32_t tick = cursorTick_ + uint32_t(btn) * g;   // pad 0 = cursor (anchor)
    if (tick >= pat.tracks[trackIdx_].lenTicks) return;  // dark bucket past end — non-interactive

    // Cursor is NOT moved: it anchors the 16-pad row.
    // Don't toggle on press. Empty bucket → add now so a hold can edit the new note;
    // a filled bucket is left alone, removed only on a quick tap (see onButtonUp).
    heldTick_        = int32_t(tick);
    heldStartMs_     = millis();
    editedWhileHeld_ = false;
    addedOnPress_    = false;
    if (!findNoteAtTick(tick, pat)) {
        addNoteAtTick(tick, pat);
        addedOnPress_ = true;
    }
}

void SequencerMode::onButtonUp(uint8_t btn, MidiIO& midi, uint8_t ch, void* context) {
    (void)midi; (void)ch;
    if (btn >= seq::BUCKETS || heldTick_ < 0) return;

    // Quick tap (held < kTapMs) with no encoder edit, on a bucket that already had a note
    // before this press → remove it. A longer hold or an edit keeps the note.
    if (context && !editedWhileHeld_ && !addedOnPress_ &&
        (millis() - heldStartMs_) < kTapMs) {
        removeNoteAtTick(uint32_t(heldTick_), *static_cast<Pattern*>(context));
    }
    heldTick_        = -1;
    editedWhileHeld_ = false;
    addedOnPress_    = false;
}

void SequencerMode::update(uint32_t now, void* context) {
    (void)now; (void)context;
}

void SequencerMode::onActivate() {
    Serial.println("[Sequencer] active");
}

void SequencerMode::onDeactivate() {}

bool SequencerMode::onControl(uint8_t c, bool down, bool shift, void* context) {
    if (c == 7) { shiftPressed_ = down; return true; }
    if (!down) return false;

    if (shift && context) {
        Pattern& pat = *static_cast<Pattern*>(context);
        switch (c) {
        case 0: clearAtCursor(pat); return true;
        case 1: copyAtCursor(pat);  return true;
        case 2: pasteAtCursor(pat); return true;
        }
    }
    return false;
}

// ─── Edit parameters ────────────────────────────────────────────────────────

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

void SequencerMode::setTrack(uint8_t t) {
    trackIdx_ = (t < MAX_TRACKS) ? t : 0;
}

// ─── Navigation / zoom / length ───────────────────────────────────────────────

uint32_t SequencerMode::lastBucketStart(const Pattern& pat) const {
    const uint32_t g   = seq::gridTicks(pat.grid);
    const uint32_t len = pat.tracks[trackIdx_].lenTicks;
    return seq::alignDown(len ? len - 1 : 0, g);
}

void SequencerMode::moveCursor(int deltaTicks, Pattern& pat) {
    // Cursor = window left edge / pad 0. Clamp so it can reach the last in-track bucket.
    int64_t nt = int64_t(cursorTick_) + deltaTicks;
    const int64_t last = int64_t(lastBucketStart(pat));
    if (nt < 0)    nt = 0;
    if (nt > last) nt = last;
    cursorTick_ = uint32_t(nt);
}

void SequencerMode::onGridChanged(Pattern& pat) {
    const uint32_t g = seq::gridTicks(pat.grid);
    cursorTick_ = seq::alignDown(cursorTick_, g);
    const uint32_t last = lastBucketStart(pat);
    if (cursorTick_ > last) cursorTick_ = last;
}

void SequencerMode::editTrackLen(int deltaTicks, Pattern& pat) {
    int64_t nl = int64_t(pat.tracks[trackIdx_].lenTicks) + deltaTicks;
    if (nl < int64_t(seq::TRACK_LEN_MIN_TICKS)) nl = seq::TRACK_LEN_MIN_TICKS;
    if (nl > int64_t(seq::TRACK_LEN_MAX_TICKS)) nl = seq::TRACK_LEN_MAX_TICKS;
    pat.tracks[trackIdx_].lenTicks = uint32_t(nl);

    const uint32_t last = lastBucketStart(pat);
    if (cursorTick_ > last) cursorTick_ = last;

    Serial.printf("[Sequencer] T%u len -> %lu ticks (%ld steps)\n",
                  trackIdx_, (unsigned long)pat.tracks[trackIdx_].lenTicks,
                  (long)(pat.tracks[trackIdx_].lenTicks / seq::TRACK_LEN_MIN_TICKS));
}

// ─── Bucket editing (act at cursor) ─────────────────────────────────────────────

// Exact onset match: a bucket maps to a note only when the note starts exactly there.
// Resolution-independent (compares absolute ticks).
void SequencerMode::addNoteAtTick(uint32_t tick, Pattern& pat) {
    NotePool<128>& pool = pat.tracks[trackIdx_].recorded;
    if (findNoteAtTick(tick, pat)) return;   // already a note here
    Note n{};
    n.on       = tick;
    n.duration = seq::gridTicks(pat.grid);   // one grid cell
    n.pitch    = editPitch_;
    n.vel      = editVelocity_;
    n.micro_q8 = 0;
    n.flags    = 0;
    if (!pool.push(n)) { Serial.println("[Sequencer] pool full"); return; }
    pool.sortByOnTick();
    char buf[5];
    Serial.printf("[Sequencer] trk%u %lu ON  (%s added)\n",
                  trackIdx_, (unsigned long)tick, pitchName(n.pitch, buf, sizeof(buf)));
}

void SequencerMode::removeNoteAtTick(uint32_t tick, Pattern& pat) {
    NotePool<128>& pool = pat.tracks[trackIdx_].recorded;
    Note* existing = findNoteAtTick(tick, pat);
    if (!existing) return;
    char buf[5];
    Serial.printf("[Sequencer] trk%u %lu OFF (%s removed)\n",
                  trackIdx_, (unsigned long)tick, pitchName(existing->pitch, buf, sizeof(buf)));
    for (uint16_t i = 0; i < pool.count; ++i)
        if (&pool.notes[i] == existing) { pool.removeAt(i); break; }
}

void SequencerMode::toggleAtTick(uint32_t tick, Pattern& pat) {
    if (findNoteAtTick(tick, pat)) removeNoteAtTick(tick, pat);
    else                           addNoteAtTick(tick, pat);
}

void SequencerMode::copyAtCursor(Pattern& pat) {
    Note* n = findNoteAtTick(cursorTick_, pat);
    if (n) {
        copyBuffer_ = *n;
        hasCopy_    = true;
        char buf[5];
        Serial.printf("[Sequencer] copied %lu (%s)\n",
                      (unsigned long)cursorTick_, pitchName(n->pitch, buf, sizeof(buf)));
    } else {
        Serial.printf("[Sequencer] %lu empty\n", (unsigned long)cursorTick_);
    }
}

void SequencerMode::pasteAtCursor(Pattern& pat) {
    if (!hasCopy_) { Serial.println("[Sequencer] copy buffer empty"); return; }
    NotePool<128>& pool = pat.tracks[trackIdx_].recorded;
    for (uint16_t i = 0; i < pool.count; ) {
        if (pool.notes[i].on == cursorTick_) pool.removeAt(i);
        else ++i;
    }
    Note pasted     = copyBuffer_;
    pasted.on       = cursorTick_;
    pasted.duration = seq::gridTicks(pat.grid);
    if (pool.push(pasted)) {
        pool.sortByOnTick();
        char buf[5];
        Serial.printf("[Sequencer] pasted %lu (%s)\n",
                      (unsigned long)cursorTick_, pitchName(pasted.pitch, buf, sizeof(buf)));
    }
}

void SequencerMode::clearAtCursor(Pattern& pat) {
    NotePool<128>& pool = pat.tracks[trackIdx_].recorded;
    uint16_t before = pool.count;
    for (uint16_t i = 0; i < pool.count; ) {
        if (pool.notes[i].on == cursorTick_) pool.removeAt(i);
        else ++i;
    }
    Serial.printf(pool.count < before ? "[Sequencer] cleared %lu\n" : "[Sequencer] %lu empty\n",
                  (unsigned long)cursorTick_);
}

// ─── Serial state display (visible window of focused track) ──────────────────────

void SequencerMode::printTrackState(const Pattern& pat) const {
    const NotePool<128>& pool = pat.tracks[trackIdx_].recorded;
    const uint8_t  ch = pat.tracks[trackIdx_].channel;
    const uint32_t g  = seq::gridTicks(pat.grid);

    char pitchBuf[5];
    Serial.printf("Trk%u ch%u  grid 1/%u  cur %d:%d  len %lu  notes:%u\n",
                  trackIdx_, ch, pat.grid,
                  seq::barOf(cursorTick_), seq::beatOf(cursorTick_),
                  (unsigned long)pat.tracks[trackIdx_].lenTicks, pool.count);

    for (uint8_t b = 0; b < seq::BUCKETS; ++b) {
        uint32_t tick   = cursorTick_ + uint32_t(b) * g;
        bool     active = false;
        for (uint16_t i = 0; i < pool.count; ++i)
            if (pool.notes[i].on == tick) { active = true; break; }
        Serial.print(active ? "[X]" : "[ ]");
    }
    Serial.println();

    for (uint8_t b = 0; b < seq::BUCKETS; ++b) {
        uint32_t tick = cursorTick_ + uint32_t(b) * g;
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

Note* SequencerMode::findNoteAtTick(uint32_t tick, Pattern& pat) {
    NotePool<128>& pool = pat.tracks[trackIdx_].recorded;
    for (uint16_t i = 0; i < pool.count; ++i)
        if (pool.notes[i].on == tick) return &pool.notes[i];
    return nullptr;
}

void SequencerMode::editHeld(uint8_t param, int8_t delta, Pattern& pat) {
    if (heldTick_ < 0 || delta == 0) return;
    Note* n = findNoteAtTick((uint32_t)heldTick_, pat);
    if (!n) return;

    editedWhileHeld_ = true;
    NotePool<128>& pool = pat.tracks[trackIdx_].recorded;

    switch (param) {
    case 0: { // pitch
        int16_t p = (int16_t)n->pitch + delta;
        n->pitch = (uint8_t)(p < 0 ? 0 : (p > 127 ? 127 : p));
        char buf[5];
        Serial.printf("[Hold] %ld pitch -> %s\n", (long)heldTick_, pitchName(n->pitch, buf, sizeof(buf)));
        break;
    }
    case 1: { // velocity
        int16_t v = (int16_t)n->vel + delta;
        n->vel = (uint8_t)(v < 1 ? 1 : (v > 127 ? 127 : v));
        Serial.printf("[Hold] %ld vel -> %u\n", (long)heldTick_, n->vel);
        break;
    }
    case 2: { // tick nudge
        int32_t t = (int32_t)n->on + delta;
        n->on = (uint32_t)(t < 0 ? 0 : t);
        pool.sortByOnTick();
        Serial.printf("[Hold] %ld tick -> %lu\n", (long)heldTick_, (unsigned long)n->on);
        break;
    }
    case 3: { // duration in grid cells
        uint32_t tps = seq::gridTicks(pat.grid);
        if (tps == 0) break;
        int32_t cur  = (int32_t)(n->duration / tps);
        int32_t next = cur + delta;
        if (next < 1)   next = 1;
        if (next > 128) next = 128;
        n->duration = (uint32_t)next * tps;
        Serial.printf("[Hold] %ld dur -> %d cells\n", (long)heldTick_, (int)next);
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
