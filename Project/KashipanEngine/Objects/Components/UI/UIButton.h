#pragma once
#include <algorithm>
#include <limits>
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
#include "Objects/Components/Render/TextRenderer.h"
#include "Objects/Components/Transform.h"
#include "Utilities/UUID128.h"
#if defined(USE_IMGUI)
#include "Objects/Components/Render/TargetObjectSelector.h"
#include "Utilities/Translation.h"
#endif

namespace KashipanEngine {

/// @brief 画面上の矩形に対するマウスのホバー・押下・クリック判定を行うUI用コンポーネント
/// @details 見た目の描画は一切行わない、いわば「透明な当たり判定レイヤー」。
///          同オブジェクトにSpriteRendererがあればそのスプライトの矩形を、無くTextRendererが
///          あればそのテキストの外接矩形（表示中の文字列・フォントサイズに応じて動的に変わる）を
///          そのまま当たり判定に使う。両方無い場合は常に判定falseを返す（SpriteRendererを優先）。
///          描画先ウィンドウは同オブジェクトのSpriteRenderer/TextRenderer側（SetTargetObject）で
///          指定されたものをそのまま使うため、このコンポーネント自体はウィンドウの参照を持たない。
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

    /// @brief 直近のUpdate()時点での、対象矩形（SpriteRenderer/TextRenderer）基準のローカルUV座標を取得する
    /// @details (0,0)=矩形の左下 ～ (1,1)=矩形の右上。矩形の外（負値や1超過）の値もそのまま返るため、
    ///          IsPressed()中にカーソルが矩形の外へ出た場合でも位置を追い続けられる（スライダー等の
    ///          ドラッグ操作の実装に使える）。ウィンドウ/表示カメラ/対象コンポーネントのいずれかが
    ///          解決できず座標を計算できなかった場合はfalseを返す（outUVは変更しない）
    bool TryGetLocalHoverPosition(Vector2 &outUV) const {
        if (!hasValidLocalUV_) return false;
        outUV = lastLocalUV_;
        return true;
    }

protected:
    void Update() override {
        isClicked_ = false;

        hasValidLocalUV_ = ComputeLocalUV(lastLocalUV_);
        const bool hoveredNow = hasValidLocalUV_ &&
            lastLocalUV_.x >= 0.0f && lastLocalUV_.x <= 1.0f &&
            lastLocalUV_.y >= 0.0f && lastLocalUV_.y <= 1.0f;
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
        if (hasValidLocalUV_) {
            ImGui::Text("Local UV: (%.3f, %.3f)", lastLocalUV_.x, lastLocalUV_.y);
        }
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
    IWindowObjectComponent *ResolveWindow(EmptyObject *targetObj) const {
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

    /// @brief TextRendererの各文字インスタンス（ワールド行列）から、テキスト全体の外接矩形
    ///        （ワールド空間AABB）を求める
    /// @details 各文字は単位クアッド（-0.5～0.5）をworldMatrixで変換したもの。回転・傾斜（イタリック）
    ///          や文字ごとのオフセット/回転オーバーライドが付いていても、4隅を変換して外接させるため
    ///          破綻しない（回転が大きい場合は矩形がやや大きめになる程度）
    static bool ComputeTextWorldBounds(const TextRenderer *textRenderer, Vector3 &outMin, Vector3 &outMax) {
        const auto instances = textRenderer->GetRenderInstances();
        if (instances.empty()) return false;

        const Vector3 corners[4] = {
            Vector3(-0.5f, -0.5f, 0.0f), Vector3(0.5f, -0.5f, 0.0f),
            Vector3(-0.5f, 0.5f, 0.0f), Vector3(0.5f, 0.5f, 0.0f),
        };

        float minX = std::numeric_limits<float>::max();
        float maxX = std::numeric_limits<float>::lowest();
        float minY = std::numeric_limits<float>::max();
        float maxY = std::numeric_limits<float>::lowest();
        for (const auto &instance : instances) {
            for (const auto &corner : corners) {
                const Vector3 p = corner.Transform(instance.worldMatrix);
                minX = std::min(minX, p.x);
                maxX = std::max(maxX, p.x);
                minY = std::min(minY, p.y);
                maxY = std::max(maxY, p.y);
            }
        }
        outMin = Vector3(minX, minY, 0.0f);
        outMax = Vector3(maxX, maxY, 0.0f);
        return true;
    }

    /// @brief 現在のマウス座標を、ボタンの矩形（同オブジェクトのSpriteRenderer/TextRendererの表示範囲）
    ///        基準のローカルUV座標（(0,0)=左下～(1,1)=右上、範囲外もありうる）へ変換する
    /// @details ScreenBufferViewport::TryGetOffscreenMousePositionと同じ変換パイプライン
    ///          （Windowクライアント座標 → NDC → 表示カメラの逆ビュー射影でワールド座標）でマウスの
    ///          ワールド座標を求めるところまでは共通。そこから先のローカルUVへの変換方法だけ対象
    ///          コンポーネントで分かれる： SpriteRendererはスプライトの逆ワールド行列でローカル座標へ
    ///          変換する（矩形サイズはTransformのスケールに追従）。TextRendererは表示中の文字列から
    ///          都度測った外接矩形（ワールド空間）を基準にする（矩形サイズは文字列・フォントサイズに
    ///          追従して動的に変わる）
    bool ComputeLocalUV(Vector2 &outUV) const {
        auto *objectContext = GetOwnerObjectContext();
        if (!objectContext) return false;

        auto *spriteRenderer = objectContext->GetComponent<SpriteRenderer>();
        auto *textRenderer = spriteRenderer ? nullptr : objectContext->GetComponent<TextRenderer>();
        if (!spriteRenderer && !textRenderer) return false;

        EmptyObject *targetObj = spriteRenderer ? spriteRenderer->GetTargetObject() : textRenderer->GetTargetObject();
        auto *windowComponent = ResolveWindow(targetObj);
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

        if (spriteRenderer) {
            // 3. ワールド座標 → ボタンのローカル座標（-0.5～0.5の単位クアッド空間）
            const Matrix4x4 spriteWorldInverse = spriteRenderer->GetWorldMatrix().Inverse();
            const Vector3 localPos = worldPos.Transform(spriteWorldInverse);

            // 4. ローカル座標を単位クアッド空間から(0,0)=左下～(1,1)=右上のUVへ変換する
            outUV.x = localPos.x + 0.5f;
            outUV.y = localPos.y + 0.5f;
            return true;
        }

        Vector3 aabbMin, aabbMax;
        if (!ComputeTextWorldBounds(textRenderer, aabbMin, aabbMax)) return false;
        const float width = aabbMax.x - aabbMin.x;
        const float height = aabbMax.y - aabbMin.y;
        if (width <= 0.0f || height <= 0.0f) return false;
        outUV.x = (worldPos.x - aabbMin.x) / width;
        outUV.y = (worldPos.y - aabbMin.y) / height;
        return true;
    }

    UUID128 displayCameraObjectID_{};
    bool isHovered_ = false;
    bool isPressed_ = false;
    bool isClicked_ = false;
    Vector2 lastLocalUV_{ 0.0f, 0.0f };
    bool hasValidLocalUV_ = false;
};

REGISTER_COMPONENT_OBJECT(UIButton)

} // namespace KashipanEngine
