#include "DestroyEvent.h"
#include <Windows.h>
#include "Core/WindowsAPI/WindowDescriptor.h"

namespace KashipanEngine {
namespace WindowEventDefault {

std::optional<LRESULT> DestroyEvent::OnEvent(UINT /*msg*/, WPARAM /*wparam*/, LPARAM /*lparam*/) {
    // PostQuitMessage はここでは送らない。全ウィンドウ破棄後に送る。
    return 0;
}

} // namespace WindowEventDefault
} // namespace KashipanEngine
