#include "IWindowEvent.h"
#include "Core/Window.h"
#include "Debug/Logger.h"

namespace KashipanEngine {

const WindowDescriptor &IWindowEvent::GetWindowDescriptor() const {
    LogScope scope;
    return window_->descriptor_;
}

const WindowSize &IWindowEvent::GetWindowSize() const {
    LogScope scope;
    return window_->size_;
}

} // namespace KashipanEngine