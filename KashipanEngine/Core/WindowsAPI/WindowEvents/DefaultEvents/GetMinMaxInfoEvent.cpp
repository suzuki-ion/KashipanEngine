#include "GetMinMaxInfoEvent.h"
#include <Windows.h>
#include "Core/Window.h"

namespace KashipanEngine {
namespace WindowEventDefault {

std::optional<LRESULT> GetMinMaxInfoEvent::OnEvent(UINT msg, WPARAM /*wparam*/, LPARAM lparam) {
    if (msg != kTargetMessage_) return std::nullopt;

    auto *window = GetWindow();
    if (!window || window->GetSizeChangeMode() != SizeChangeMode::FixedAspect) return std::nullopt;

    auto *minMaxInfo = reinterpret_cast<MINMAXINFO *>(lparam);
    float aspectRatio = window->GetAspectRatio();

    int minWidth = 320;
    int minHeight = static_cast<int>(minWidth / aspectRatio);
    minMaxInfo->ptMinTrackSize.x = minWidth;
    minMaxInfo->ptMinTrackSize.y = minHeight;

    RECT workArea{};
    SystemParametersInfo(SPI_GETWORKAREA, 0, &workArea, 0);
    int maxWidth = workArea.right - workArea.left;
    int maxHeight = static_cast<int>(maxWidth / aspectRatio);
    if (maxHeight > workArea.bottom - workArea.top) {
        maxHeight = workArea.bottom - workArea.top;
        maxWidth = static_cast<int>(maxHeight * aspectRatio);
    }
    minMaxInfo->ptMaxTrackSize.x = maxWidth;
    minMaxInfo->ptMaxTrackSize.y = maxHeight;

    return 0;
}

} // namespace WindowEventDefault
} // namespace KashipanEngine
