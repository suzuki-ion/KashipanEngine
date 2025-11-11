#pragma once
#include <optional>
#include <memory>
#include "Core/WindowsAPI/WindowEvents/IWindowEvent.h"

namespace KashipanEngine {
namespace WindowEventDefault {

// Alt+F4 / タイトルバーのX などの SC_CLOSE を捕捉
class SysCommandCloseEvent final : public IWindowEvent {
public:
    SysCommandCloseEvent() : IWindowEvent(WM_SYSCOMMAND) {}
    std::optional<LRESULT> OnEvent(UINT msg, WPARAM wparam, LPARAM lparam) override;
    std::unique_ptr<IWindowEvent> Clone() override { return std::make_unique<SysCommandCloseEvent>(*this); }
};

} // namespace WindowEventDefault
} // namespace KashipanEngine
