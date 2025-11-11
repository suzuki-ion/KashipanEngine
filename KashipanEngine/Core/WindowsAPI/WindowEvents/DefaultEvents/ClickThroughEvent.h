#pragma once
#include <optional>
#include <memory>
#include "Core/WindowsAPI/WindowEvents/IWindowEvent.h"

namespace KashipanEngine {
namespace WindowEventDefault {

/// @brief クリック透過イベント（WM_NCHITTESTでHTTRANSPARENTを返す）
class ClickThroughEvent final : public IWindowEvent {
public:
    explicit ClickThroughEvent(bool enable = true)
        : IWindowEvent(WM_NCHITTEST), enable_(enable) {}

    /// @brief 有効/無効を切り替え
    void SetEnable(bool enable) { enable_ = enable; }

    std::optional<LRESULT> OnEvent(UINT msg, WPARAM wparam, LPARAM lparam) override;
    std::unique_ptr<IWindowEvent> Clone() override { return std::make_unique<ClickThroughEvent>(*this); }

private:
    bool enable_ = true;
};

} // namespace WindowEventDefault
} // namespace KashipanEngine
