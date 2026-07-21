#pragma once
#include <cstdint>
#include <functional>
#include <string>
#include <unordered_set>
#include <vector>

#include "Objects/ObjectComponentHeader.h"
#include "Core/Window.h"
#include "Scene/RenderTargetCarryOverRegistry.h"

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

    //==================================================
    // メッセージの横取り（インターセプト）
    //==================================================

    /// @brief 指定のメッセージを横取りするかを設定する
    /// @details 横取り対象のメッセージはウィンドウの既定処理（エンジン既定イベント・DefWindowProc）が
    ///          実行されなくなり、OnWindowMessage通知だけが行われる。ウィンドウの処理を中断して
    ///          ゲーム側の処理へ差し替えたいメッセージにのみ使用する。
    ///          WM_CLOSEを横取りするとXボタン・Alt+F4でウィンドウが閉じなくなるため、
    ///          ゲーム側の処理が済んでから CloseWindow() で明示的に閉じること。
    ///          WM_NCHITTESTやWM_SYSCOMMAND等を横取りするとウィンドウの基本動作
    ///          （ドラッグ移動・リサイズ・最小化等）が壊れる点に注意
    /// @return 設定できた場合はtrue（WM_DESTROY等の横取り不可メッセージはfalse）
    bool SetMessageIntercepted(std::uint32_t msg, bool enabled) {
        if (!Window::IsInterceptableMessage(msg)) return false;
        if (enabled) interceptedMessages_.insert(msg);
        else interceptedMessages_.erase(msg);
        if (window_ && Window::IsExist(window_)) window_->SetMessageIntercepted(msg, enabled);
        return true;
    }
    /// @brief 指定のメッセージを横取りするかを取得する
    bool IsMessageIntercepted(std::uint32_t msg) const { return interceptedMessages_.contains(msg); }

    /// @brief ウィンドウを閉じる（破棄予約する）
    /// @details WM_CLOSEを横取りしている場合に、ゲーム側の処理が済んでから実際に閉じるために使う
    void CloseWindow() {
        if (window_ && Window::IsExist(window_)) window_->DestroyNotify();
    }

protected:
    IWindowObjectComponent(const std::string &typeName, size_t maxCount, size_t componentTypeID, std::string defaultTitle)
        : IObjectComponent(typeName, maxCount, componentTypeID), title_(std::move(defaultTitle)) {
        // title_の直接書き込み時は、セッターと同様に生成済みウィンドウのタイトルへも反映する
        ADD_MEMBER_VARIABLE_WITH_CALLBACK(title_, [this] { SetTitle(title_); });
        ADD_MEMBER_VARIABLE(width_);
        ADD_MEMBER_VARIABLE(height_);
    }

    /// @brief 横取り設定を所有ウィンドウへ適用する（派生クラスのInitializeでウィンドウ生成後に呼ぶ）
    void ApplyInterceptedMessages() {
        if (!window_ || !Window::IsExist(window_)) return;
        for (const auto msg : interceptedMessages_) {
            window_->SetMessageIntercepted(msg, true);
        }
    }

    /// @brief 所有ウィンドウを破棄する（派生クラスのFinalizeから呼ぶ）
    /// @details シーン切り替え中は即座に破棄せず、次のシーンの同名ウィンドウコンポーネントへの
    ///          引き継ぎ候補として預ける（切り替え中でなければ登録側が即座に破棄する）
    /// @param kind このコンポーネント種別に対応する引き継ぎプールの種別
    void DepositWindowForCarryOver(RenderTargetCarryOverRegistry::Kind kind) {
        if (window_ && Window::IsExist(window_)) {
            RenderTargetCarryOverRegistry::Deposit(kind, title_, window_,
                [w = window_] { if (Window::IsExist(w)) w->DestroyNotify(); });
        }
        window_ = nullptr;
    }
    /// @brief 前のシーンから引き継がれたウィンドウがあれば、Initializeで生成済みのウィンドウと
    ///        差し替えて引き取る（派生クラスのLoadFromJsonでtitle_確定後に呼ぶ）
    /// @param kind このコンポーネント種別に対応する引き継ぎプールの種別
    /// @return 引き継ぎが行われた場合は true
    bool TryClaimCarriedOverWindow(RenderTargetCarryOverRegistry::Kind kind) {
        if (title_.empty()) return false;
        auto *claimed = static_cast<Window *>(RenderTargetCarryOverRegistry::Claim(kind, title_));
        if (!claimed) return false;
        if (window_ && Window::IsExist(window_)) window_->DestroyNotify();
        window_ = claimed;
        window_->SetWindowTitle(title_);
        return true;
    }

    /// @brief 横取り設定をJSON（数値配列）へ保存する（派生クラスのSaveToJsonから呼ぶ）
    JSON SaveInterceptedMessagesJson() const {
        JSON json = JSON::array();
        for (const auto msg : interceptedMessages_) {
            json.push_back(msg);
        }
        return json;
    }
    /// @brief 横取り設定をJSON（数値配列）から読み込み、ウィンドウが生成済みなら適用する
    void LoadInterceptedMessagesJson(const JSON &json) {
        // 既にウィンドウへ適用済みの分は一旦解除してから読み込む
        if (window_ && Window::IsExist(window_)) {
            for (const auto msg : interceptedMessages_) {
                window_->SetMessageIntercepted(msg, false);
            }
        }
        interceptedMessages_.clear();
        if (json.is_array()) {
            for (const auto &value : json) {
                if (value.is_number_integer() || value.is_number_unsigned()) {
                    const auto msg = value.get<std::uint32_t>();
                    if (Window::IsInterceptableMessage(msg)) interceptedMessages_.insert(msg);
                }
            }
        }
        ApplyInterceptedMessages();
    }

#if defined(USE_IMGUI)
    /// @brief 横取りメッセージの編集UI（派生クラスのShowImGuiから呼ぶ）
    void ShowInterceptedMessagesImGui() {
        if (!ImGui::TreeNode("Intercepted Messages")) return;
        ImGui::PushTextWrapPos(0.0f);
        ImGui::TextDisabled("横取り対象のメッセージは既定処理が実行されず、スクリプトのOnWindowMessage通知のみ行われます");
        ImGui::PopTextWrapPos();

        bool interceptClose = IsMessageIntercepted(WM_CLOSE);
        if (ImGui::Checkbox("Intercept Close (WM_CLOSE)", &interceptClose)) {
            SetMessageIntercepted(WM_CLOSE, interceptClose);
        }
        if (ImGui::IsItemHovered()) {
            ImGui::SetTooltip("Xボタン・Alt+F4で閉じなくなります。スクリプトからCloseWindow()で閉じてください");
        }

        std::uint32_t removeTarget = 0;
        bool hasRemoveTarget = false;
        for (const auto msg : interceptedMessages_) {
            ImGui::PushID(static_cast<int>(msg));
            ImGui::Text("0x%04X (%u)", msg, msg);
            ImGui::SameLine();
            if (ImGui::SmallButton("Remove")) {
                removeTarget = msg;
                hasRemoveTarget = true;
            }
            ImGui::PopID();
        }
        if (hasRemoveTarget) SetMessageIntercepted(removeTarget, false);

        ImGui::SetNextItemWidth(120.0f);
        ImGui::InputInt("##InterceptMsgInput", &pendingInterceptMessage_, 0);
        ImGui::SameLine();
        if (ImGui::Button("Add") && pendingInterceptMessage_ > 0) {
            SetMessageIntercepted(static_cast<std::uint32_t>(pendingInterceptMessage_), true);
        }
        ImGui::TreePop();
    }
#endif

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
    /// @brief 横取り対象のメッセージ（ウィンドウ再生成時にも引き継ぐためコンポーネント側でも保持する）
    std::unordered_set<std::uint32_t> interceptedMessages_;

private:
    WindowMessageCallback onWindowMessage_;
#if defined(USE_IMGUI)
    /// @brief ImGuiのメッセージ番号入力用の一時値
    int pendingInterceptMessage_ = 0;
#endif
};

} // namespace KashipanEngine
