#pragma once
#include <cstdint>
#include <functional>
#include <string>
#include <vector>

#include "Objects/ObjectComponentHeader.h"
#include "Core/Window.h"

namespace KashipanEngine {

/// @brief ウィンドウを生成・所有するコンポーネント（NormalWindowObject/OverlayWindowObject）の基底クラス
/// @details ウィンドウが受信したメッセージをコールバックで通知する仕組みを持つ。
///          ScriptComponentはこのコールバックをフックして、スクリプトの
///          OnWindowMessage(const WindowMessageInfo &in) メソッドを呼び出す。
class IWindowObjectComponent : public IObjectComponent {
public:
    // 派生クラスは全て Render/RenderTarget カテゴリに分類される
    COMPONENT_CATEGORY("Render", "RenderTarget")

    /// @brief ウィンドウメッセージ通知の情報
    struct WindowMessageEvent {
        /// @brief 通知元のウィンドウコンポーネント（どのコンポーネントのウィンドウかの識別用）
        IWindowObjectComponent *sourceComponent = nullptr;
        UINT message = 0;
        WPARAM wparam = 0;
        LPARAM lparam = 0;
    };
    using WindowMessageCallback = std::function<void(const WindowMessageEvent &)>;

    /// @brief 所有しているウィンドウを取得する（未生成の場合は nullptr）
    Window *GetWindow() const noexcept { return window_; }

    void SetTitle(const std::string &title) {
        if (window_ && Window::IsExist(window_)) window_->SetWindowTitle(title);
        title_ = title;
    }
    const std::string &GetTitle() const noexcept { return title_; }
    void SetSize(std::uint32_t width, std::uint32_t height) {
        width_ = width;
        height_ = height;
    }

    //==================================================
    // ウィンドウメッセージ通知コールバック
    //==================================================

    /// @brief ウィンドウメッセージ通知コールバックを設定する（ScriptComponent等がフックする）
    void SetOnWindowMessage(WindowMessageCallback callback) { onWindowMessage_ = std::move(callback); }
    const WindowMessageCallback &GetOnWindowMessage() const noexcept { return onWindowMessage_; }

protected:
    IWindowObjectComponent(const std::string &typeName, size_t maxCount, size_t componentTypeID, std::string defaultTitle)
        : IObjectComponent(typeName, maxCount, componentTypeID), title_(std::move(defaultTitle)) {}

    /// @brief このフレームにウィンドウが受信したメッセージをコールバックへ通知する（派生クラスのUpdateから呼ぶ）
    /// @details メッセージはウィンドウがメッセージ種別ごとに最後の1件を保持したものが対象で、通知順は不定。
    ///          コールバック内からのウィンドウ操作（SetWindowSize等）で同期的にメッセージが再入し
    ///          保持マップが変化する可能性があるため、一覧をコピーしてから通知する
    void DispatchWindowMessages() {
        if (!onWindowMessage_) return;
        if (!window_ || !Window::IsExist(window_)) return;

        std::vector<WindowMessage> messages;
        messages.reserve(window_->GetMessages().size());
        for (const auto &pair : window_->GetMessages()) {
            messages.push_back(pair.second);
        }
        for (const auto &message : messages) {
            WindowMessageEvent event;
            event.sourceComponent = this;
            event.message = message.msg;
            event.wparam = message.wparam;
            event.lparam = message.lparam;
            onWindowMessage_(event);
        }
    }

    Window *window_ = nullptr;
    std::string title_;
    std::uint32_t width_ = 1280;
    std::uint32_t height_ = 720;

private:
    WindowMessageCallback onWindowMessage_;
};

} // namespace KashipanEngine
