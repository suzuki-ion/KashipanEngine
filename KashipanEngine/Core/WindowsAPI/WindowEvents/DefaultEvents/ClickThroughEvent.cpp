#include "ClickThroughEvent.h"
#include <Windows.h>

namespace KashipanEngine {
namespace WindowEventDefault {

std::optional<LRESULT> ClickThroughEvent::OnEvent(UINT /*msg*/, WPARAM /*wparam*/, LPARAM /*lparam*/) {
    if (!enable_) {
        return std::nullopt;
    }
    // 全面クリック透過。条件付きにしたい場合は座標判定を追加可能。
    // POINT pt{ GET_X_LPARAM(lparam), GET_Y_LPARAM(lparam) }; ScreenToClient(GetWindow()->GetWindowHandle(), &pt);
    return HTTRANSPARENT;
}

} // namespace WindowEventDefault
} // namespace KashipanEngine
