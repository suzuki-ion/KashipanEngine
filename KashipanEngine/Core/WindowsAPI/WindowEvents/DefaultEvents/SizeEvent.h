#pragma once
#include <optional>
#include <memory>
#include "Core/WindowsAPI/WindowEvents/IWindowEvent.h"

namespace KashipanEngine {
namespace WindowEventDefault {

class SizeEvent final : public IWindowEvent {
public:
    SizeEvent() : IWindowEvent(WM_SIZE) {}
    std::optional<LRESULT> OnEvent(UINT msg, WPARAM wparam, LPARAM lparam) override;
    std::unique_ptr<IWindowEvent> Clone() override { return std::make_unique<SizeEvent>(*this); }
};

} // namespace WindowEventDefault
} // namespace KashipanEngine
