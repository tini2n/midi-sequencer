#pragma once
#include <stdint.h>
#include "../model/pattern.hpp"
#include "../core/transport.hpp"
#include "../io/cursor_mode.hpp"

// Read-only snapshot of sequencer state consumed by all views.
// Built once per App::update() and passed down — views never write to model.
struct UICtx {
    const Pattern&    pat;
    const Transport&  transport;
    const CursorMode& cursor;
    uint32_t          now;          // micros()
    bool              settingsMode;
};
