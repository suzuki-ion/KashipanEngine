#include "SizeEvent.h"
#include <Windows.h>
#include "Core/WindowsAPI/WindowDescriptor.h"
#include "Core/WindowsAPI/WindowSize.h"

namespace KashipanEngine {
namespace WindowEventDefault {

std::optional<LRESULT> SizeEvent::OnEvent(UINT /*msg*/, WPARAM /*wparam*/, LPARAM lparam) {
    UINT width = LOWORD(lparam);
    UINT height = HIWORD(lparam);
    WindowSize &size = GetWindowSizeRef();
    size.clientWidth = static_cast<int32_t>(width);
    size.clientHeight = static_cast<int32_t>(height);
    RecalculateAspectRatio();

    return std::nullopt;
}

} // namespace WindowEventDefault
} // namespace KashipanEngine
