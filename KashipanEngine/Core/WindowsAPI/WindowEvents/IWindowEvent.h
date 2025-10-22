#pragma once
#include <Windows.h>
#include <cstdint>
#include <optional>
#include <memory>

namespace KashipanEngine {

class Window;
struct WindowDescriptor;
struct WindowSize;

/// @brief ウィンドウイベント用のインターフェース
class IWindowEvent {
public:
    virtual ~IWindowEvent() = default;
protected:
    IWindowEvent(UINT targetMessage) : kTargetMessage_(targetMessage) {}
    const UINT kTargetMessage_ = 0;

    /// @brief ウィンドウ取得
    Window *GetWindow() const { return window_; }
    /// @brief ウィンドウ情報取得
    const WindowDescriptor &GetWindowDescriptor() const;
    /// @brief ウィンドウサイズ情報取得
    const WindowSize &GetWindowSize() const;
    /// @brief ウィンドウ情報取得（編集可能）
    WindowDescriptor &GetWindowDescriptorRef();
    /// @brief ウィンドウサイズ情報取得（編集可能）
    WindowSize &GetWindowSizeRef();
    /// @brief アスペクト比を再計算
    void RecalculateAspectRatio();

private:
    friend class Window;
    Window *window_ = nullptr;
    void SetWindow(Window *window) { window_ = window; }

    /// @brief イベントを検知したときの処理
    /// @return ハンドル済みの場合は結果を返す。未処理の場合はstd::nulloptを返す。
    virtual std::optional<LRESULT> OnEvent(UINT msg, WPARAM wparam, LPARAM lparam) = 0;

    /// @brief 自身のクローンを生成
    virtual std::unique_ptr<IWindowEvent> Clone() = 0;
};

} // namespace KashipanEngine
