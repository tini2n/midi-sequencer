#include "matrix_kb.hpp"
#include "ui/views/view_manager.hpp"

// 0: Rec arm toggle (was Play/Pause)
// 1: Play/Pause (was Rec arm toggle)
// 2: Stop (was Rec arm toggle)
// 3: (unused)
// 4: (unused)
// 5: (unused)
// 6: Switch to next view
// 7: Shift (reserved)


void MatrixKB::onControl(uint8_t c, bool down)
{
    // Log to USB Serial for visibility
    Serial.printf("CTL %u %s\n", c, down ? "DOWN" : "UP");
    
    // Default transport/view control logic (only on press)
    if (!down) return;
    
    switch (c)
    {
    case 0: // Rec arm toggle
        if (rec_)
        {
            bool newState = !rec_->isArmed();
            rec_->arm(newState);
            Serial.printf("REC %s\n", newState ? "ARMED" : "DISARMED");
        }
        break;
    case 1: // Play/Pause
        if (rl_)
        {
            bool running = tx_ && tx_->isRunning();
            rl_->post(AppEvent{running ? AppEvent::Type::Pause : AppEvent::Type::Play});
            Serial.println(running ? "POST: Pause" : "POST: Play");
        }
        break;
    case 2: // Stop
        if (rl_)
        {
            rl_->post(AppEvent{AppEvent::Type::Stop});
            Serial.println("POST: Stop");
        }
        break;
    case 3:
    case 4:
    case 5:
        break;
    case 6: // View switch
        if (vm_)
        {
            vm_->switchToNextView();
        }
        break;
    case 7: // Shift (handled by mode if needed)
        break;
    default:
        break;
    }
}