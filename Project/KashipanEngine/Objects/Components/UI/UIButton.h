#pragma once
#include <memory>

#include "Objects/ObjectComponentHeader.h"
#include "Core/Window.h"
#include "Input/Input.h"
#include "Input/Mouse.h"
#include "Input/MouseButton.h"
#include "Math/Matrix4x4.h"
#include "Math/Vector2.h"
#include "Math/Vector3.h"
#include "Objects/Components/Render/Camera2D.h"
#include "Objects/Components/Render/IWindowObjectComponent.h"
#include "Objects/Components/Render/NormalWindowObject.h"
#include "Objects/Components/Render/OverlayWindowObject.h"
#include "Objects/Components/Render/SpriteRenderer.h"
#include "Objects/Components/Transform.h"
#include "Utilities/UUID128.h"
#if defined(USE_IMGUI)
#include "Objects/Components/Render/TargetObjectSelector.h"
#include "Utilities/Translation.h"
#endif

namespace KashipanEngine {

/// @brief 画面上の矩形に対するマウスのホバー・押下・クリック判定を行うUI用コンポーネント
/// @details 見た目の描画は一切行わない（同オブジェクトのSpriteRendererが描画するスプライトの
///          矩形をそのまま当たり判定に使う、いわば「透明な当たり判定レイヤー」）。
///          SpriteRendererが無い場合は常に判定falseを返す。
///          描画先ウィンドウは同オブジェクトのSpriteRenderer側（SetTargetObject）で指定された
///          ものをそのまま使うため、このコンポーネント自体はウィンドウの参照を持たない。
///          マウス座標変換の基準にするCamera2Dのみ、SetDisplayCameraObjectで別途指定する
///          （ScreenBufferViewportのマウス座標変換と同じ考え方・同じ変換パイプライン）。
class UIButton final : public IObjectComponent {
public:
    OBJECT_COMPONENT_CONSTRUCTOR(UIButton, 1, )
    COMPONENT_CATEGORY("UI")
    ~UIButton() override = default;

    std::unique_ptr<IObjectComponent> Clone() const override {
        auto ptr = std::make_unique<UIButton>();
        ptr->displayCameraObjectID_ = displayCameraObjectID_;
        return ptr;
    }

    //==================================================
    // 参照設定
    //==================================================

    /// @brief マウス座標変換の基準にするCamera2Dを持つオブジェクトを設定する
    void SetDisplayCameraObject(const EmptyObject *cameraObject) {
        displayCameraObjectID_ = cameraObject ? cameraObject->GetObjectID() : UUID128();
    }
    void SetDisplayCameraObject(const UUID128 &cameraObjectID) { displayCameraObjectID_ = cameraObjectID; }
    const UUID128 &GetDisplayCameraObjectID() const noexcept { return displayCameraObjectID_; }
    EmptyObject *GetDisplayCameraObject() const {
        auto *sceneContext = GetOwnerSceneContext();
        if (!sceneContext || !displayCameraObjectID_.IsValid()) return nullptr;
        return sceneContext->GetSceneObject(displayCameraObjectID_);
    }

    //==================================================
    // 状態取得
    //==================================================

    /// @brief マウスカーソルがボタンの矩形内にあるかどうか
    bool IsHovered() const noexcept { return isHovered_; }
    /// @brief ボタン上で左クリックが押され、まだ離されていないかどうか
    /// @details 押下開始はボタン上である必要があるが、離すまでの間はカーソルが矩形外へ
    ///          出ても保持される（ドラッグして離せばキャンセル扱いになる、一般的なUIボタンと同じ挙動）
    bool IsPressed() const noexcept { return isPressed_; }
    /// @brief このフレームでクリックが確定した瞬間かどうか（ボタン上で押して、ボタン上で離した時のみtrue）
    bool IsClicked() const noexcept { return isClicked_; }

protected:
    void Update() override {
        isClicked_ = false;

        const bool hoveredNow = ComputeIsHovered();
        isHovered_ = hoveredNow;

        auto *sceneContext = GetOwnerSceneContext();
        Input *input = sceneContext ? sceneContext->GetInput() : nullptr;
        if (!input) {
            isPressed_ = false;
            return;
        }
        Mouse &mouse = input->GetMouse();
        constexpr int kLeftButton = static_cast<int>(MouseButton::Left);

        if (!isPressed_) {
            if (hoveredNow && mouse.IsButtonTrigger(kLeftButton)) {
                isPressed_ = true;
            }
        } else if (mouse.IsButtonRelease(kLeftButton)) {
            if (hoveredNow) isClicked_ = true;
            isPressed_ = false;
        }
    }

#if defined(USE_IMGUI)
    void ShowImGui() override {
        TargetObjectSelector::ShowSelector(TranslationLabel("component.uibutton.display_camera"), GetOwnerSceneContext(), displayCameraObjectID_, true, false);
        ImGui::TextDisabled("%s", TranslationC("component.uibutton.desc"));
        ImGui::Text("Hovered: %s / Pressed: %s / Clicked: %s",
            isHovered_ ? "true" : "false", isPressed_ ? "true" : "false", isClicked_ ? "true" : "false");
    }
#endif

    JSON SaveToJson() const override {
        JSON json = JSON::object();
        json["displayCameraObjectID"] = ToJSON(displayCameraObjectID_);
        return json;
    }
    bool LoadFromJson(const JSON &json) override {
        displayCameraObjectID_ = json.contains("displayCameraObjectID") ? FromJSON<UUID128>(json["displayCameraObjectID"]) : UUID128();
        return true;
    }

private:
    IWindowObjectComponent *ResolveWindow(const SpriteRenderer *spriteRenderer) const {
        if (!spriteRenderer) return nullptr;
        EmptyObject *targetObj = spriteRenderer->GetTargetObject();
        if (!targetObj) return nullptr;
        if (auto *w = targetObj->GetComponent<NormalWindowObject>()) return w;
        if (auto *w = targetObj->GetComponent<OverlayWindowObject>()) return w;
        return nullptr;
    }

    Camera2D *ResolveDisplayCamera(Transform *&outTransform) const {
        outTransform = nullptr;
        auto *obj = GetDisplayCameraObject();
        if (!obj) return nullptr;
        outTransform = obj->GetComponent<Transform>();
        return obj->GetComponent<Camera2D>();
    }

    /// @brief 現在のマウス座標がボタンの矩形（同オブジェクトのSpriteRendererの表示範囲）内にあるかを判定する
    /// @details ScreenBufferViewport::TryGetOffscreenMousePositionと同じ変換パイプライン
    ///          （Windowクライアント座標 → NDC → 表示カメラの逆ビュー射影でワールド座標 →
    ///          スプライトの逆ワールド行列でローカル座標）を使い、最後にローカル座標が
    ///          単位クアッド範囲内かどうかだけを見る
    bool ComputeIsHovered() const {
        auto *objectContext = GetOwnerObjectContext();
        auto *spriteRenderer = objectContext ? objectContext->GetComponent<SpriteRenderer>() : nullptr;
        if (!spriteRenderer) return false;

        auto *windowComponent = ResolveWindow(spriteRenderer);
        Window *window = windowComponent ? windowComponent->GetWindow() : nullptr;
        if (!window || !Window::IsExist(window)) return false;

        Transform *cameraTransform = nullptr;
        Camera2D *camera2d = ResolveDisplayCamera(cameraTransform);
        if (!camera2d || !cameraTransform) return false;

        auto *sceneContext = GetOwnerSceneContext();
        Input *input = sceneContext ? sceneContext->GetInput() : nullptr;
        if (!input) return false;

        const float clientWidth = static_cast<float>(window->GetClientWidth());
        const float clientHeight = static_cast<float>(window->GetClientHeight());
        if (clientWidth <= 0.0f || clientHeight <= 0.0f) return false;

        // 1. Windowクライアント座標(px) → NDC（Yは画面下が-1になるよう反転）
        const POINT mousePos = input->GetMouse().GetPos(window);
        const float ndcX = (static_cast<float>(mousePos.x) / clientWidth) * 2.0f - 1.0f;
        const float ndcY = 1.0f - (static_cast<float>(mousePos.y) / clientHeight) * 2.0f;

        // 2. NDC → ワールド座標（表示カメラのビュー射影行列の逆行列）
        Matrix4x4 projection;
        projection.MakeOrthographicMatrix(
            0.0f, 0.0f, camera2d->GetWidth(), camera2d->GetHeight(),
            camera2d->GetNearClip(), camera2d->GetFarClip());
        const Matrix4x4 view = cameraTransform->GetWorldMatrix().Inverse();
        const Matrix4x4 viewProjectionInverse = (view * projection).Inverse();
        const Vector3 worldPos = Vector3(ndcX, ndcY, 0.0f).Transform(viewProjectionInverse);

        // 3. ワールド座標 → ボタンのローカル座標（-0.5～0.5の単位クアッド空間）
        const Matrix4x4 spriteWorldInverse = spriteRenderer->GetWorldMatrix().Inverse();
        const Vector3 localPos = worldPos.Transform(spriteWorldInverse);

        // 4. ローカル座標が単位クアッド範囲内（(0,0)=左下～(1,1)=右上）にあるかどうか
        const float nx = localPos.x + 0.5f;
        const float ny = localPos.y + 0.5f;
        return nx >= 0.0f && nx <= 1.0f && ny >= 0.0f && ny <= 1.0f;
    }

    UUID128 displayCameraObjectID_{};
    bool isHovered_ = false;
    bool isPressed_ = false;
    bool isClicked_ = false;
};

REGISTER_COMPONENT_OBJECT(UIButton)

} // namespace KashipanEngine
