#pragma once
#include <optional>
#include <memory>
#include "Core/WindowsAPI/WindowEvents/IWindowEvent.h"

namespace KashipanEngine {
namespace WindowEventDefault {

class CloseEvent final : public IWindowEvent {
public:
    CloseEvent() : IWindowEvent(WM_CLOSE) {}
    std::optional<LRESULT> OnEvent(UINT msg, WPARAM wparam, LPARAM lparam) override;
    std::unique_ptr<IWindowEvent> Clone() override { return std::make_unique<CloseEvent>(*this); }
};

} // namespace WindowEventDefault
} // namespace KashipanEngine
