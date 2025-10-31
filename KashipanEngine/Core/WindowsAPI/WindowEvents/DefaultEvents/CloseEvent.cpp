#include "CloseEvent.h"
#include <Windows.h>
#include "Core/WindowsAPI/Window.h"
#include "Core/WindowsAPI/WindowDescriptor.h"

namespace KashipanEngine {
namespace WindowEventDefault {

std::optional<LRESULT> CloseEvent::OnEvent(UINT /*msg*/, WPARAM /*wparam*/, LPARAM /*lparam*/) {
    auto &desc = GetWindowDescriptorRef();
    // ウィンドウ終了確認ダイアログを表示
    int result = MessageBoxA(
        desc.hwnd,
        "終了しますか？",
        "確認",
        MB_YESNO | MB_ICONQUESTION
    );
    if (result == IDYES) {
        GetWindow()->Destroy();
    }

    return 0;
}

} // namespace WindowEventDefault
} // namespace KashipanEngine
