#pragma once
#include <optional>
#include <memory>
#include "Core/WindowsAPI/WindowEvents/IWindowEvent.h"

namespace KashipanEngine {
namespace WindowEventDefault {

class EnterSizeMoveEvent final : public IWindowEvent {
public:
    EnterSizeMoveEvent() : IWindowEvent(WM_ENTERSIZEMOVE) {}
    std::optional<LRESULT> OnEvent(UINT msg, WPARAM wparam, LPARAM lparam) override;
    std::unique_ptr<IWindowEvent> Clone() override { return std::make_unique<EnterSizeMoveEvent>(*this); }
};

} // namespace WindowEventDefault
} // namespace KashipanEngine
