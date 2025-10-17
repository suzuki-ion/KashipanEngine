#pragma once
#include <optional>
#include <memory>
#include "Core/WindowsAPI/WindowEvents/IWindowEvent.h"

namespace KashipanEngine {
namespace WindowEventDefault {

class DestroyEvent final : public IWindowEvent {
public:
    DestroyEvent() : IWindowEvent(WM_DESTROY) {}
    std::optional<LRESULT> OnEvent(UINT msg, WPARAM wparam, LPARAM lparam) override;
    std::unique_ptr<IWindowEvent> Clone() override { return std::make_unique<DestroyEvent>(*this); }
};

} // namespace WindowEventDefault
} // namespace KashipanEngine
