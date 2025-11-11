#pragma once
#include <optional>
#include <memory>
#include "Core/WindowsAPI/WindowEvents/IWindowEvent.h"

namespace KashipanEngine {
namespace WindowEventDefault {

class SysCommandCloseEventSimple final : public IWindowEvent {
public:
    SysCommandCloseEventSimple() : IWindowEvent(WM_SYSCOMMAND) {}
    std::optional<LRESULT> OnEvent(UINT msg, WPARAM wparam, LPARAM lparam) override;
    std::unique_ptr<IWindowEvent> Clone() override { return std::make_unique<SysCommandCloseEventSimple>(*this); }
};

} // namespace WindowEventDefault
} // namespace KashipanEngine
