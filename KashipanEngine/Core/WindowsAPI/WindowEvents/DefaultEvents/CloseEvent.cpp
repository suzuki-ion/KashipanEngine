#include "CloseEvent.h"
#include <Windows.h>
#include "Core/WindowsAPI/WindowDescriptor.h"

namespace KashipanEngine {
namespace WindowEventDefault {

std::optional<LRESULT> CloseEvent::OnEvent(UINT msg, WPARAM /*wparam*/, LPARAM /*lparam*/) {
    if (msg != kTargetMessage_) return std::nullopt;
    auto &desc = GetWindowDescriptorRef();
    desc.isVisible = false;
    ::DestroyWindow(desc.hwnd);
    return 0;
}

} // namespace WindowEventDefault
} // namespace KashipanEngine
