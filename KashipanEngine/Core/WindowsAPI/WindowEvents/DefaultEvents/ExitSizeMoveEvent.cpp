#include "ExitSizeMoveEvent.h"
#include <Windows.h>

namespace KashipanEngine {
namespace WindowEventDefault {

std::optional<LRESULT> ExitSizeMoveEvent::OnEvent(UINT msg, WPARAM /*wparam*/, LPARAM /*lparam*/) {
    if (msg != kTargetMessage_) return std::nullopt;
    return std::nullopt;
}

} // namespace WindowEventDefault
} // namespace KashipanEngine
