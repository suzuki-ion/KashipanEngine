#include "DefaultEvents.h"
#include <Windows.h>
#include "Core/Window.h"
#include "Core/WindowsAPI/WindowDescriptor.h"

namespace KashipanEngine {
namespace WindowDefaultEvent {

std::optional<LRESULT> ActivateEvent::OnEvent(UINT msg, WPARAM /*wparam*/, LPARAM lparam) {
    static_cast<void>(msg);
    static_cast<void>(lparam);
    return std::nullopt;
}

std::optional<LRESULT> ClickThroughEvent::OnEvent(UINT /*msg*/, WPARAM /*wparam*/, LPARAM /*lparam*/) {
    if (!enable_) {
        return std::nullopt;
    }
    return HTTRANSPARENT;
}

std::optional<LRESULT> CloseEvent::OnEvent(UINT /*msg*/, WPARAM /*wparam*/, LPARAM /*lparam*/) {
    GetWindow()->Destroy();
    return 0;
}

std::optional<LRESULT> DestroyEvent::OnEvent(UINT /*msg*/, WPARAM /*wparam*/, LPARAM /*lparam*/) {
    return 0;
}

std::optional<LRESULT> EnterSizeMoveEvent::OnEvent(UINT msg, WPARAM /*wparam*/, LPARAM /*lparam*/) {
    if (msg != kTargetMessage_) return std::nullopt;
    return std::nullopt;
}

std::optional<LRESULT> ExitSizeMoveEvent::OnEvent(UINT msg, WPARAM /*wparam*/, LPARAM /*lparam*/) {
    if (msg != kTargetMessage_) return std::nullopt;
    return std::nullopt;
}

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

std::optional<LRESULT> SizingEvent::OnEvent(UINT /*msg*/, WPARAM wparam, LPARAM lparam) {
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
    return 0;
}

std::optional<LRESULT> SysCommandCloseEventSimple::OnEvent(UINT /*msg*/, WPARAM wparam, LPARAM /*lparam*/) {
    if (wparam != SC_CLOSE) {
        return std::nullopt;
    }
    GetWindow()->Destroy();
    return 0;
}

} // namespace WindowDefaultEvent
} // namespace KashipanEngine
