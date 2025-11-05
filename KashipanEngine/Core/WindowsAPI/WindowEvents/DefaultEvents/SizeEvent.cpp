#include "SizeEvent.h"
#include <Windows.h>
#include "Core/Window.h"

namespace KashipanEngine {
namespace WindowEventDefault {

std::optional<LRESULT> SizeEvent::OnEvent(UINT /*msg*/, WPARAM /*wparam*/, LPARAM lparam) {
    UINT width = LOWORD(lparam);
    UINT height = HIWORD(lparam);
    auto *window = GetWindow();
    if (!window) {
        return std::nullopt;
    }
    window->SetWindowSize(
        static_cast<int32_t>(width),
        static_cast<int32_t>(height)
    );
    return 0;
}

} // namespace WindowEventDefault
} // namespace KashipanEngine
