#pragma once
#include <optional>
#include <memory>
#include "Core/WindowsAPI/WindowEvents/IWindowEvent.h"

namespace KashipanEngine {
namespace WindowEventDefault {

class SizingEvent final : public IWindowEvent {
public:
    SizingEvent() : IWindowEvent(WM_SIZING) {}
    std::optional<LRESULT> OnEvent(UINT msg, WPARAM wparam, LPARAM lparam) override;
    std::unique_ptr<IWindowEvent> Clone() override { return std::make_unique<SizingEvent>(*this); }
};

} // namespace WindowEventDefault
} // namespace KashipanEngine
