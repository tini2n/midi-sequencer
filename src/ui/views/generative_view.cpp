#include "generative_view.hpp"
#include "io/matrix_kb_mode.hpp" // For IModeConfig

void GenerativeView::onEncoderRotation(const EncoderRotationEvent& event)
{
    Generator* gen = generatorManager_.getCurrentGenerator();
    if (!gen) return;
    
    switch (event.encoderId)
    {
        case 0: // ENC1: Density parameter
        {
            float density = 0;
            if (gen->getParameter("density", density))
            {
                density += event.delta * 5.0f; // Step by 5
                if (density < 0) density = 0;
                if (density > 100) density = 100;
                generatorManager_.setParameter("density", density);
                Serial.printf("[GenerativeView] ENC1 Density: %.0f\n", density);
            }
            break;
        }
        case 1: // ENC2: Length parameter
        {
            float length = 0;
            if (gen->getParameter("length", length))
            {
                length += event.delta;
                if (length < 1) length = 1;
                if (length > 64) length = 64;
                generatorManager_.setParameter("length", length);
                Serial.printf("[GenerativeView] ENC2 Length: %.0f\n", length);
            }
            break;
        }
        case 2: // ENC3: Edit pitch for cursor mode
        {
            if (!mkb_) break;
            // Control cursor mode edit pitch
            int newPitch = (int)cachedEditPitch_ + event.delta;
            if (newPitch < 0) newPitch = 0;
            if (newPitch > 127) newPitch = 127;
            
            // Update cached edit pitch only; MatrixKB has no mode API here
            cachedEditPitch_ = (uint8_t)newPitch;
            
            // Also update generator base_note for convenience
            float baseNote = 0;
            if (gen->getParameter("base_note", baseNote))
            {
                generatorManager_.setParameter("base_note", (float)newPitch);
            }
            break;
        }
        case 3: // ENC4: Switch generator
        {
            if (event.delta > 0)
                switchToNextGenerator();
            else if (event.delta < 0)
                switchToPreviousGenerator();
            break;
        }
        case 4: // ENC5-8: Reserved for future parameters
        case 5:
        case 6:
        case 7:
        {
            Serial.printf("[GenerativeView] ENC%d delta: %d (not assigned)\n", 
                         event.encoderId + 1, event.delta);
            break;
        }
    }
}

void GenerativeView::onEncoderButton(const EncoderButtonEvent& event)
{
    if (event.pressed)
    {
        switch (event.encoderId)
        {
            case 0: // ENC1 SW: Reset density to default
            case 1: // ENC2 SW: Reset length to default
            case 2: // ENC3 SW: Reset base_note to default
            {
                resetToDefaults();
                Serial.printf("[GenerativeView] ENC%d SW: Reset all to defaults\n", 
                             event.encoderId + 1);
                break;
            }
            case 3: // ENC4 SW: List available generators
                generatorManager_.printAvailableGenerators();
                break;
            case 4: // ENC5-8 SW: Reserved
            case 5:
            case 6:
            case 7:
                Serial.printf("[GenerativeView] ENC%d SW pressed (not assigned)\n", 
                             event.encoderId + 1);
                break;
        }
    }
}

void GenerativeView::begin(uint8_t midiChannel)
{
    midiChannel_ = midiChannel;
    
    // Initialize generator manager
    generatorManager_.begin();
    
    Serial.println("GenerativeView initialized");
}

void GenerativeView::attach(RunLoop* runLoop, RecordEngine* recordEngine, Transport* transport, class ViewManager* viewManager)
{
    runLoop_ = runLoop;
    recordEngine_ = recordEngine;
    transport_ = transport;
    (void)viewManager; // Stored in MatrixKB directly
}


void GenerativeView::draw(Pattern& pattern, Viewport& viewport, OledRenderer& oled, 
                         MidiIO& midi, uint32_t now, uint32_t playTick)
{
    (void)midi;
    
    // Create HUD string for generative view
    char hud[64];
    drawHUD(hud, sizeof(hud));
    
    // Use the piano roll for pattern visualization
    PianoRoll::Options options = {};
    // Use cached edit pitch (updated by encoder handler, not here)
    options.highlightPitch = cachedEditPitch_;
    
    oled.rollSetOptions(options);
    oled.drawFrame(pattern, viewport, now, playTick, hud);
}

void GenerativeView::poll(MidiIO& midi)
{
    // Matrix keyboard now acts as step sequencer cursor in this view
    if (mkb_) mkb_->poll(midi, midiChannel_, nullptr);
}

void GenerativeView::onActivate()
{
    // Initialize cached edit pitch default
    cachedEditPitch_ = 60;
    
    Serial.println("=== GenerativeView Activated ===");
    Serial.println("Serial Commands:");
    Serial.println("  'g' - Generate pattern");
    Serial.println("  'n' - Next generator");
    Serial.println("  'b' - Previous generator");
    Serial.println("  'l' - List generators");
    Serial.println("  'i' - Generator info & parameters");
    Serial.println("  'r' - Reset to defaults");
    Serial.println("  'S<param> <value>' - Set parameter");
    Serial.println("    Examples: Sdensity 80, Slength 16");
    Serial.println("Matrix KB (Cursor Mode):");
    Serial.println("  Buttons 0-15 - Select step + toggle note");
    Serial.println("  ENC3 - Edit pitch");
    Serial.println("  Shift + Rec - Clear step");
    Serial.println("  Shift + Play - Copy step");
    Serial.println("  Shift + Stop - Paste step");
    Serial.println("  Button 1 - Play/Pause");
    Serial.println("  Button 2 - Stop");
    Serial.println("  Button 6 - Switch views");
    Serial.println("================================");
    
    generatorManager_.printCurrentGenerator();
    lastGenTime_ = micros();
}

void GenerativeView::onDeactivate()
{
    Serial.println("GenerativeView deactivated");
}

void GenerativeView::triggerGeneration(Pattern& pattern)
{
    isGenerating_ = true;
    lastGenTime_ = micros();
    
    generatorManager_.generatePattern(pattern);
    
    isGenerating_ = false;
}

void GenerativeView::switchToNextGenerator()
{
    generatorManager_.switchToNextGenerator();
}

void GenerativeView::switchToPreviousGenerator()
{
    generatorManager_.switchToPreviousGenerator();
}

void GenerativeView::resetToDefaults()
{
    generatorManager_.resetCurrentGeneratorToDefaults();
}

void GenerativeView::drawHUD(char* buffer, size_t bufferSize) const
{
    Generator* gen = generatorManager_.getCurrentGenerator();
    if (gen) {
        // Get some key parameters for display
        float density = 0, length = 0;
        gen->getParameter("density", density);
        gen->getParameter("length", length);
        
        snprintf(buffer, bufferSize, "GEN:%s D:%.0f L:%.0f %s", 
                 gen->getShortName(),
                 density,
                 length,
                 isGenerating_ ? "..." : "RDY");
    } else {
        snprintf(buffer, bufferSize, "GEN: NO GENERATOR");
    }
}