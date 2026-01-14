#pragma once
#include <optional>
#include <memory>
#include "Core/WindowsAPI/WindowEvents/IWindowEvent.h"

namespace KashipanEngine {
namespace WindowDefaultEvent {

/// @brief アクティベートイベント（WM_ACTIVATE）
class ActivateEvent final : public IWindowEvent {
public:
    ActivateEvent() : IWindowEvent(WM_ACTIVATE) {}
    std::optional<LRESULT> OnEvent(UINT msg, WPARAM wparam, LPARAM lparam) override;
    std::unique_ptr<IWindowEvent> Clone() override { return std::make_unique<ActivateEvent>(*this); }
};

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

/// @brief クローズイベント（WM_CLOSE）
class CloseEvent final : public IWindowEvent {
public:
    CloseEvent() : IWindowEvent(WM_CLOSE) {}
    std::optional<LRESULT> OnEvent(UINT msg, WPARAM wparam, LPARAM lparam) override;
    std::unique_ptr<IWindowEvent> Clone() override { return std::make_unique<CloseEvent>(*this); }
};

/// @brief デストロイイベント（WM_DESTROY）
class DestroyEvent final : public IWindowEvent {
public:
    DestroyEvent() : IWindowEvent(WM_DESTROY) {}
    std::optional<LRESULT> OnEvent(UINT msg, WPARAM wparam, LPARAM lparam) override;
    std::unique_ptr<IWindowEvent> Clone() override { return std::make_unique<DestroyEvent>(*this); }
};

/// @brief サイズ変更開始イベント（WM_ENTERSIZEMOVE）
class EnterSizeMoveEvent final : public IWindowEvent {
public:
    EnterSizeMoveEvent() : IWindowEvent(WM_ENTERSIZEMOVE) {}
    std::optional<LRESULT> OnEvent(UINT msg, WPARAM wparam, LPARAM lparam) override;
    std::unique_ptr<IWindowEvent> Clone() override { return std::make_unique<EnterSizeMoveEvent>(*this); }
};

/// @brief サイズ変更終了イベント（WM_EXITSIZEMOVE）
class ExitSizeMoveEvent final : public IWindowEvent {
public:
    ExitSizeMoveEvent() : IWindowEvent(WM_EXITSIZEMOVE) {}
    std::optional<LRESULT> OnEvent(UINT msg, WPARAM wparam, LPARAM lparam) override;
    std::unique_ptr<IWindowEvent> Clone() override { return std::make_unique<ExitSizeMoveEvent>(*this); }
};

/// @brief 最小/最大サイズ情報取得イベント（WM_GETMINMAXINFO）
class GetMinMaxInfoEvent final : public IWindowEvent {
public:
    GetMinMaxInfoEvent() : IWindowEvent(WM_GETMINMAXINFO) {}
    std::optional<LRESULT> OnEvent(UINT msg, WPARAM wparam, LPARAM lparam) override;
    std::unique_ptr<IWindowEvent> Clone() override { return std::make_unique<GetMinMaxInfoEvent>(*this); }
};

/// @brief サイズ変更イベント（WM_SIZE）
class SizeEvent final : public IWindowEvent {
public:
    SizeEvent() : IWindowEvent(WM_SIZE) {}
    std::optional<LRESULT> OnEvent(UINT msg, WPARAM wparam, LPARAM lparam) override;
    std::unique_ptr<IWindowEvent> Clone() override { return std::make_unique<SizeEvent>(*this); }
};

/// @brief サイズ変更中イベント（WM_SIZING）
class SizingEvent final : public IWindowEvent {
public:
    SizingEvent() : IWindowEvent(WM_SIZING) {}
    std::optional<LRESULT> OnEvent(UINT msg, WPARAM wparam, LPARAM lparam) override;
    std::unique_ptr<IWindowEvent> Clone() override { return std::make_unique<SizingEvent>(*this); }
};

// Alt+F4 / タイトルバーのX などの SC_CLOSE を捕捉
class SysCommandCloseEvent final : public IWindowEvent {
public:
    SysCommandCloseEvent() : IWindowEvent(WM_SYSCOMMAND) {}
    std::optional<LRESULT> OnEvent(UINT msg, WPARAM wparam, LPARAM lparam) override;
    std::unique_ptr<IWindowEvent> Clone() override { return std::make_unique<SysCommandCloseEvent>(*this); }
};

/// @brief シンプルな Alt+F4 / タイトルバーのX などの SC_CLOSE 捕捉イベント
class SysCommandCloseEventSimple final : public IWindowEvent {
public:
    SysCommandCloseEventSimple() : IWindowEvent(WM_SYSCOMMAND) {}
    std::optional<LRESULT> OnEvent(UINT msg, WPARAM wparam, LPARAM lparam) override;
    std::unique_ptr<IWindowEvent> Clone() override { return std::make_unique<SysCommandCloseEventSimple>(*this); }
};

} // namespace WindowDefaultEvent
} // namespace KashipanEngine
