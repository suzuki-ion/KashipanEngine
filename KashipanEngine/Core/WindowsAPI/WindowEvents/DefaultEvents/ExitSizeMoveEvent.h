#pragma once
#include <optional>
#include <memory>
#include "Core/WindowsAPI/WindowEvents/IWindowEvent.h"

namespace KashipanEngine {
namespace WindowEventDefault {

class ExitSizeMoveEvent final : public IWindowEvent {
public:
    ExitSizeMoveEvent() : IWindowEvent(WM_EXITSIZEMOVE) {}
    std::optional<LRESULT> OnEvent(UINT msg, WPARAM wparam, LPARAM lparam) override;
    std::unique_ptr<IWindowEvent> Clone() override { return std::make_unique<ExitSizeMoveEvent>(*this); }
};

} // namespace WindowEventDefault
} // namespace KashipanEngine
