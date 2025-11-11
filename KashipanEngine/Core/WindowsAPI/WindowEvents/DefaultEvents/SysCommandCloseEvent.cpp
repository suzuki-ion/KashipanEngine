#include "SysCommandCloseEvent.h"
#include <Windows.h>
#include "Core/Window.h"
#include "Core/WindowsAPI/WindowDescriptor.h"

namespace KashipanEngine {
namespace WindowEventDefault {

std::optional<LRESULT> SysCommandCloseEvent::OnEvent(UINT /*msg*/, WPARAM wparam, LPARAM /*lparam*/) {
    if (wparam != SC_CLOSE) {
        return std::nullopt;
    }
    int result = MessageBoxA(
        GetWindowDescriptor().hwnd,
        "終了しますか？",
        "確認",
        MB_YESNO | MB_ICONQUESTION
    );
    if (result == IDYES) {
        GetWindow()->Destroy();
    }
    // SC_CLOSE を常にハンドル済みにする (NOなら何もしないで閉じ抑止)
    return 0;
}

} // namespace WindowEventDefault
} // namespace KashipanEngine
