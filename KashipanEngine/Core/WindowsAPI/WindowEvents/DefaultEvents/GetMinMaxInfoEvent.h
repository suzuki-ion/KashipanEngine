#pragma once
#include <optional>
#include <memory>
#include "Core/WindowsAPI/WindowEvents/IWindowEvent.h"

namespace KashipanEngine {
namespace WindowEventDefault {

class GetMinMaxInfoEvent final : public IWindowEvent {
public:
    GetMinMaxInfoEvent() : IWindowEvent(WM_GETMINMAXINFO) {}
    std::optional<LRESULT> OnEvent(UINT msg, WPARAM wparam, LPARAM lparam) override;
    std::unique_ptr<IWindowEvent> Clone() override { return std::make_unique<GetMinMaxInfoEvent>(*this); }
};

} // namespace WindowEventDefault
} // namespace KashipanEngine
