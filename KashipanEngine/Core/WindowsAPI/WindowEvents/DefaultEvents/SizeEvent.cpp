#include "SizeEvent.h"
#include <Windows.h>
#include "Core/WindowsAPI/WindowDescriptor.h"
#include "Core/WindowsAPI/WindowSize.h"

namespace KashipanEngine {
namespace WindowEventDefault {

std::optional<LRESULT> SizeEvent::OnEvent(UINT msg, WPARAM wparam, LPARAM lparam) {
    if (msg != kTargetMessage_) return std::nullopt;

    auto &desc = GetWindowDescriptorRef();
    auto &size = GetWindowSizeRef();

    UINT width = LOWORD(lparam);
    UINT height = HIWORD(lparam);

    if (wparam == SIZE_MINIMIZED) {
        desc.isMinimized = true;
        desc.isMaximized = false;
        desc.isVisible = false;
    } else if (wparam == SIZE_MAXIMIZED) {
        desc.isMinimized = false;
        desc.isMaximized = true;
        desc.isVisible = true;
        size.clientWidth = static_cast<int32_t>(width);
        size.clientHeight = static_cast<int32_t>(height);
        RecalculateAspectRatio();
    } else if (wparam == SIZE_RESTORED) {
        desc.isMinimized = false;
        desc.isMaximized = false;
        desc.isVisible = true;
        size.clientWidth = static_cast<int32_t>(width);
        size.clientHeight = static_cast<int32_t>(height);
        RecalculateAspectRatio();
    }
    return std::nullopt; // let DefWindowProc run if needed
}

} // namespace WindowEventDefault
} // namespace KashipanEngine
