#include "IWindowEvent.h"
#include "Core/WindowsAPI/Window.h"

namespace KashipanEngine {

const WindowDescriptor &IWindowEvent::GetWindowDescriptor() const {
    return window_->descriptor_;
}

const WindowSize &IWindowEvent::GetWindowSize() const {
    return window_->size_;
}

WindowDescriptor &IWindowEvent::GetWindowDescriptorRef() {
    return window_->descriptor_;
}

WindowSize &IWindowEvent::GetWindowSizeRef() {
    return window_->size_;
}

void IWindowEvent::RecalculateAspectRatio() {
    window_->CalculateAspectRatio();
}

} // namespace KashipanEngine