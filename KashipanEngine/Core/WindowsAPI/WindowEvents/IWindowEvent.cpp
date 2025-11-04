#include "IWindowEvent.h"
#include "Core/Window.h"
#include "Debug/Logger.h"

namespace KashipanEngine {

const WindowDescriptor &IWindowEvent::GetWindowDescriptor() const {
    // const メンバはスコープログ生成しない
    return window_->descriptor_;
}

const WindowSize &IWindowEvent::GetWindowSize() const {
    return window_->size_;
}

WindowDescriptor &IWindowEvent::GetWindowDescriptorRef() {
    LogScope scope;
    return window_->descriptor_;
}

WindowSize &IWindowEvent::GetWindowSizeRef() {
    LogScope scope;
    return window_->size_;
}

void IWindowEvent::RecalculateAspectRatio() {
    LogScope scope;
    window_->CalculateAspectRatio();
}

} // namespace KashipanEngine