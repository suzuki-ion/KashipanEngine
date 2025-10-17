#pragma once
#include <optional>
#include <memory>
#include "Core/WindowsAPI/WindowEvents/IWindowEvent.h"

namespace KashipanEngine {
namespace WindowEventDefault {

class ActivateEvent final : public IWindowEvent {
public:
    ActivateEvent() : IWindowEvent(WM_ACTIVATE) {}
    std::optional<LRESULT> OnEvent(UINT msg, WPARAM wparam, LPARAM lparam) override;
    std::unique_ptr<IWindowEvent> Clone() override { return std::make_unique<ActivateEvent>(*this); }
};

} // namespace WindowEventDefault
} // namespace KashipanEngine
