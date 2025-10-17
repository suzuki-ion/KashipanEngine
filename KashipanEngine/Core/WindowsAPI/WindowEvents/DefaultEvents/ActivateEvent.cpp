#include "ActivateEvent.h"
#include <Windows.h>
#include "Core/WindowsAPI/WindowDescriptor.h"

namespace KashipanEngine {
namespace WindowEventDefault {

std::optional<LRESULT> ActivateEvent::OnEvent(UINT msg, WPARAM wparam, LPARAM /*lparam*/) {
    if (msg != kTargetMessage_) return std::nullopt;
    auto &desc = GetWindowDescriptorRef();
    desc.isActive = (LOWORD(wparam) != WA_INACTIVE);
    return std::nullopt;
}

} // namespace WindowEventDefault
} // namespace KashipanEngine
