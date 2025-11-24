#include "matrix_kb.hpp"
#include "ui/views/view_manager.hpp"

// Control row mapping:
// 0: Rec arm toggle (or Shift+Rec = Clear in cursor mode)
// 1: Play/Pause (or Shift+Play = Copy in cursor mode)
// 2: Stop (or Shift+Stop = Paste in cursor mode)
// 3: (unused)
// 4: (unused)
// 5: (unused)
// 6: Switch to next view
// 7: Shift modifier

void MatrixKB::onControl(uint8_t c, bool down)
{
    // Log to USB Serial for visibility
    Serial.printf("CTL %u %s\n", c, down ? "DOWN" : "UP");

    // Handle Shift button state (Ctl 7)
    if (c == 7)
    {
        cursorMode_.setShiftPressed(down);
        Serial.printf("SHIFT %s\n", down ? "DOWN" : "UP");
        return;
    }

    if (!down)
        return; // only on press for other controls

    // Check if shift is pressed and we're in cursor mode
    bool shiftActive = (mode_ == Mode::Cursor && cursorMode_.getSelectedStep() < 16);

    switch (c)
    {
    case 0: // Rec arm toggle OR Shift+Rec = Clear step
        if (shiftActive && pattern_)
        {
            cursorMode_.clearStep(*pattern_);
        }
        else if (rec_)
        {
            bool newState = !rec_->isArmed();
            rec_->arm(newState);
            Serial.printf("REC %s\n", newState ? "ARMED" : "DISARMED");
        }
        break;

    case 1: // Play/Pause OR Shift+Play = Copy step
        if (shiftActive && pattern_)
        {
            cursorMode_.copyStep(*pattern_);
        }
        else if (rl_)
        {
            bool running = tx_ && tx_->isRunning();
            rl_->post(AppEvent{running ? AppEvent::Type::Pause : AppEvent::Type::Play});
            Serial.println(running ? "POST: Pause" : "POST: Play");
        }
        break;

    case 2: // Stop OR Shift+Stop = Paste step
        if (shiftActive && pattern_)
        {
            cursorMode_.pasteToStep(*pattern_);
        }
        else if (rl_)
        {
            rl_->post(AppEvent{AppEvent::Type::Stop});
            Serial.println("POST: Stop");
        }
        break;

    case 3:
        break;
    case 4:
        break;
    case 5:
        break;
    case 6:
        if (vm_)
        {
            vm_->switchToNextView();
        }
        break;
    default:
        break;
    }
}