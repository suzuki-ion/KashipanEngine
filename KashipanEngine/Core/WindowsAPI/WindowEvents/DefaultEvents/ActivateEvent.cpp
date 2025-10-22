#include "ActivateEvent.h"
#include <Windows.h>
#include "Core/WindowsAPI/WindowDescriptor.h"

namespace KashipanEngine {
namespace WindowEventDefault {

std::optional<LRESULT> ActivateEvent::OnEvent(UINT msg, WPARAM /*wparam*/, LPARAM lparam) {
    static_cast<void>(msg);
    static_cast<void>(lparam);
    return std::nullopt;
}

} // namespace WindowEventDefault
} // namespace KashipanEngine
