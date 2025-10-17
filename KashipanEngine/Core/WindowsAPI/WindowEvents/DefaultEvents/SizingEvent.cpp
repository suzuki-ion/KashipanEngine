#include "SizingEvent.h"
#include <Windows.h>
#include "Core/WindowsAPI/Window.h"

namespace KashipanEngine {
namespace WindowEventDefault {

std::optional<LRESULT> SizingEvent::OnEvent(UINT msg, WPARAM wparam, LPARAM lparam) {
    if (msg != kTargetMessage_) return std::nullopt;

    auto *window = GetWindow();
    if (!window || window->GetSizeChangeMode() != SizeChangeMode::FixedAspect) return std::nullopt;

    auto *rect = reinterpret_cast<RECT *>(lparam);
    float aspectRatio = window->GetAspectRatio();

    int width = rect->right - rect->left;
    int height = rect->bottom - rect->top;

    switch (wparam) {
        case WMSZ_LEFT:
        case WMSZ_RIGHT:
            height = static_cast<int>(width / aspectRatio);
            rect->bottom = rect->top + height;
            break;
        case WMSZ_TOP:
        case WMSZ_BOTTOM:
            width = static_cast<int>(height * aspectRatio);
            rect->right = rect->left + width;
            break;
        case WMSZ_TOPLEFT:
        case WMSZ_TOPRIGHT:
        case WMSZ_BOTTOMLEFT:
        case WMSZ_BOTTOMRIGHT: {
            int newHeight = static_cast<int>(width / aspectRatio);
            if (newHeight != height) {
                rect->bottom = rect->top + newHeight;
            }
            break;
        }
    }

    return std::nullopt;
}

} // namespace WindowEventDefault
} // namespace KashipanEngine
