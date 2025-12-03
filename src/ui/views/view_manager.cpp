#include "view_manager.hpp"

void ViewManager::registerView(ViewType viewType, IView* view)
{
    if (view == nullptr) {
        Serial.println("Warning: Attempted to register null view");
        return;
    }
    
    size_t index = static_cast<size_t>(viewType);
    if (index >= views_.size()) {
        Serial.println("Warning: Invalid view type in registerView");
        return;
    }
    
    views_[index] = view;
    
    // If this is the first view registered, make it the current view
    if (currentView_ == nullptr && viewType == currentViewType_) {
        currentView_ = view;
    }
}

void ViewManager::beginAll(uint8_t midiChannel)
{
    for (auto* view : views_) {
        if (view != nullptr) {
            view->begin(midiChannel);
        }
    }
}

void ViewManager::attachAll(RunLoop* runLoop, RecordEngine* recordEngine, Transport* transport)
{
    for (auto* view : views_) {
        if (view != nullptr) {
            // Pass this ViewManager as the 4th parameter for view switching support
            view->attach(runLoop, recordEngine, transport, this);
        }
    }
}

bool ViewManager::switchToView(ViewType viewType)
{
    size_t index = static_cast<size_t>(viewType);
    if (index >= views_.size() || views_[index] == nullptr) {
        Serial.printf("Warning: Cannot switch to unregistered view type %d\n", static_cast<int>(viewType));
        return false;
    }
    
    if (viewType == currentViewType_) {
        return true; // Already on this view
    }
    
    // Deactivate current view
    if (currentView_ != nullptr) {
        currentView_->onDeactivate();
    }
    
    // Switch to new view
    currentViewType_ = viewType;
    currentView_ = views_[index];
    
    // Activate new view
    if (currentView_ != nullptr) {
        currentView_->onActivate();
        
        // Set encoder handler for the newly activated view if encoders are initialized
        if (encodersInitialized_) {
            encoderMgr_.setHandler(currentView_->getEncoderHandler());
        }
    }
    
    Serial.printf("Switched to view: %s\n", getCurrentViewName());
    return true;
}

void ViewManager::activateView(ViewType viewType)
{
    switchToView(viewType);
}

void ViewManager::switchToNextView()
{
    // Switch between Performance and Generative views only
    ViewType nextViewType;
    if (currentViewType_ == ViewType::Performance) {
        nextViewType = ViewType::Generative;
    } else {
        nextViewType = ViewType::Performance;
    }
    
    switchToView(nextViewType);
}

void ViewManager::switchToPreviousView()
{
    // Same logic as next view since we only have 2 views
    switchToNextView();
}

void ViewManager::draw(Pattern& pattern, Viewport& viewport, OledRenderer& oled, 
                      MidiIO& midi, uint32_t now, uint32_t playTick)
{
    if (currentView_ != nullptr) {
        currentView_->draw(pattern, viewport, oled, midi, now, playTick);
    }
}

void ViewManager::poll(MidiIO& midi)
{
    if (currentView_ != nullptr) {
        currentView_->poll(midi);
    }
    
    // Poll encoders if initialized
    if (encodersInitialized_) {
        pollEncoders();
    }
}

void ViewManager::beginEncoders(const EncoderManager::PinConfig configs[EncoderManager::NUM_ENCODERS], 
                                uint32_t debounceUs)
{
    encoderMgr_.begin(configs, debounceUs);
    encodersInitialized_ = true;
    Serial.println("Encoder manager initialized");
}

void ViewManager::pollEncoders()
{
    // Handler was already set in activateView(), just poll
    encoderMgr_.poll();
}

const char* ViewManager::getCurrentViewName() const
{
    if (currentView_ != nullptr) {
        return currentView_->getName();
    }
    return "None";
}