#pragma once
#include <algorithm>
#include <cstdint>
#include <string>

#include "Objects/ObjectComponentHeader.h"
#include "Objects/Components/Render/IWindowObjectComponent.h"
#include "Objects/Components/Transform.h"
#include "Core/Window.h"
#include "Utilities/Translation.h"

namespace KashipanEngine {

class NormalWindowObject final : public IWindowObjectComponent {
public:
    NormalWindowObject()
        : IWindowObjectComponent("NormalWindowObject", 0xFF, GetComponentTypeID<NormalWindowObject>(), "Normal Window") {}
    ~NormalWindowObject() override = default;

    std::unique_ptr<IObjectComponent> Clone() const override {
        auto ptr = std::make_unique<NormalWindowObject>();
        ptr->title_ = title_;
        ptr->iconPath_ = iconPath_;
        ptr->width_ = width_;
        ptr->height_ = height_;
        ptr->syncWithTransform_ = syncWithTransform_;
        ptr->interceptedMessages_ = interceptedMessages_;
        return ptr;
    }

protected:
    void Initialize() override {
        if (window_) return;
        // シーン切り替え中は、直後のLoadFromJsonでtitle_が確定してから前のシーンの
        // 同名ウィンドウを引き継げる可能性がある。ここで既定タイトルのウィンドウを
        // 先に生成してしまうと、生成直後に破棄予約される一時ウィンドウがOS上で
        // 一瞬表示されてしまうため、生成自体をLoadFromJson側に委ねる
        if (RenderTargetCarryOverRegistry::IsSceneSwitchInProgress()) return;
        // iconPath_が未設定（既定）の場合はCreateNormal自身の既定アイコンを使わせるため、
        // 空文字を明示的に渡さない（EngineSettings.jsonのinitialWindowIconPathは既定で空のため、
        // 渡してしまうとエンジンロゴアイコンではなくOS既定アイコンになってしまう）
        window_ = iconPath_.empty()
            ? Window::CreateNormal(title_, static_cast<int32_t>(width_), static_cast<int32_t>(height_))
            : Window::CreateNormal(title_, static_cast<int32_t>(width_), static_cast<int32_t>(height_), 0, iconPath_);
        // メッセージの横取り設定を生成したウィンドウへ適用する
        ApplyInterceptedMessages();
    }
    void Update() override {
        // ウィンドウが受信したメッセージをコールバック（スクリプト等）へ通知する
        DispatchWindowMessages();

        if (!window_ || !Window::IsExist(window_)) return;
        if (!syncWithTransform_) return;
        auto *ownerContext = GetOwnerObjectContext();
        if (!ownerContext) return;
        auto *tr = ownerContext->GetComponent<Transform>();
        if (!tr) return;
        // ワールド行列から位置とスケールを取得してウィンドウに反映
        auto worldMatrix = tr->GetWorldMatrix();
        Vector3 pos(worldMatrix.m[3][0], worldMatrix.m[3][1], worldMatrix.m[3][2]);
        Vector3 scale(
            Vector3(worldMatrix.m[0][0], worldMatrix.m[0][1], worldMatrix.m[0][2]).Length(),
            Vector3(worldMatrix.m[1][0], worldMatrix.m[1][1], worldMatrix.m[1][2]).Length(),
            Vector3(worldMatrix.m[2][0], worldMatrix.m[2][1], worldMatrix.m[2][2]).Length()
        );
        int32_t posX = static_cast<int32_t>(pos.x);
        int32_t posY = static_cast<int32_t>(pos.y);
        int32_t width = static_cast<int32_t>(width_) * static_cast<int32_t>(scale.x);
        int32_t height = static_cast<int32_t>(height_) * static_cast<int32_t>(scale.y);
        if (width < 1) width = 1;
        if (height < 1) height = 1;
        if (window_->GetWindowPosition().x != posX || window_->GetWindowPosition().y != posY) {
            window_->SetWindowPosition(posX, posY);
        }
        if (window_->GetClientWidth() != width || window_->GetClientHeight() != height) {
            window_->SetWindowSize(width, height);
        }
    }
    void Finalize() override {
        DepositWindowForCarryOver(RenderTargetCarryOverRegistry::Kind::NormalWindow);
    }

#if defined(USE_IMGUI)
    void ShowImGui() override {
        if (ImGuiCustom::EditValue(TranslationLabel("component.normalwindowobject.title"), title_)) SetTitle(title_);
        if (ImGuiCustom::EditValue(TranslationLabel("component.normalwindowobject.icon"), iconPath_)) SetIcon(iconPath_);
        int w = static_cast<int>(width_);
        int h = static_cast<int>(height_);
        if (ImGuiCustom::EditValue(TranslationLabel("component.normalwindowobject.width"), w)) width_ = static_cast<std::uint32_t>(std::max(1, w));
        if (ImGuiCustom::EditValue(TranslationLabel("component.normalwindowobject.height"), h)) height_ = static_cast<std::uint32_t>(std::max(1, h));
        ImGui::Checkbox(TranslationLabel("component.normalwindowobject.sync_with_transform"), &syncWithTransform_);
        ShowInterceptedMessagesImGui();
        if (!Window::IsExist(window_)) {
            if (ImGui::Button(TranslationLabel("component.normalwindowobject.respawn_window"))) {
                window_ = nullptr;
                Initialize();
            }
        }
    }
#endif

    JSON SaveToJson() const override {
        JSON json = JSON::object();
        json["title"] = title_;
        json["iconPath"] = iconPath_;
        json["width"] = width_;
        json["height"] = height_;
        json["syncWithTransform"] = syncWithTransform_;
        json["interceptedMessages"] = SaveInterceptedMessagesJson();
        return json;
    }

    bool LoadFromJson(const JSON &json) override {
        title_ = json.value("title", std::string{ "Normal Window" });
        iconPath_ = json.value("iconPath", std::string{});
        width_ = json.value("width", 1280u);
        height_ = json.value("height", 720u);
        syncWithTransform_ = json.value("syncWithTransform", true);
        LoadInterceptedMessagesJson(json.value("interceptedMessages", JSON::array()));

        // AddComponent() はプール上の実体を所有オブジェクトへ登録する前に、渡された一時
        // インスタンスの状態を JSON 経由で一度転送する。この内部転送では既定タイトルの
        // "Normal Window" が入る場合があり、ここで実ウィンドウを生成すると、直後の本ロードで
        // 同名ウィンドウへ差し替えられる一時ウィンドウが表示されてしまう。
        // 登録後の本ロード、または登録後に明示的に状態を復元する経路でのみ外部リソースを扱う。
        if (!IsRegisteredToOwner()) return true;

        // 前のシーンから引き継がれたウィンドウがあればそちらを使う。
        // 無い場合、Initializeで既に生成済み（シーン切り替え中でない通常ケース）ならそのタイトル・
        // アイコンを実際の設定値へ合わせ、未生成（シーン切り替え中でIntializeが生成を保留していた
        // ケース）ならここで確定したtitle_で新規生成する
        if (!TryClaimCarriedOverWindow(RenderTargetCarryOverRegistry::Kind::NormalWindow)) {
            if (window_ && Window::IsExist(window_)) {
                window_->SetWindowTitle(title_);
                // iconPath_が空の場合はInitializeで設定済みの既定アイコンをそのまま残す
                if (!iconPath_.empty()) window_->SetIcon(iconPath_);
            } else if (IsActive()) {
                // シーン切り替え中でIntializeでの生成を保留していた場合、引き継ぎ候補も無かったので
                // ここで確定したtitle_で生成する（一時的な既定タイトルのウィンドウを経由しないため
                // 生成→即破棄によるちらつきが起きない）。非アクティブな場合はここでも生成せず、
                // 従来通りSetActive(true)によるInitialize()呼び出しまで生成を保留する
                window_ = iconPath_.empty()
                    ? Window::CreateNormal(title_, static_cast<int32_t>(width_), static_cast<int32_t>(height_))
                    : Window::CreateNormal(title_, static_cast<int32_t>(width_), static_cast<int32_t>(height_), 0, iconPath_);
                ApplyInterceptedMessages();
            }
        }
        // JSONから同期設定を読み込んだ後にのみ、初回の位置・サイズ同期を行う
        if (syncWithTransform_) Update();
        return true;
    }
};

REGISTER_COMPONENT_OBJECT(NormalWindowObject);

} // namespace KashipanEngine
