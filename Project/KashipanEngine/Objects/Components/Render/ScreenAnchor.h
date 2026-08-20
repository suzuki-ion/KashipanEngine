#pragma once
#include <memory>

#include "Objects/ObjectComponentHeader.h"
#include "Math/Vector2.h"
#include "Math/Vector3.h"
#include "Objects/Components/Render/Camera2D.h"
#include "Objects/Components/Transform.h"
#include "Utilities/UUID128.h"
#if defined(USE_IMGUI)
#include "Objects/Components/Render/TargetObjectSelector.h"
#include "Utilities/Translation.h"
#endif

namespace KashipanEngine {

/// @brief 同オブジェクトのTransformを、指定したCamera2Dの画面（width/height）上の基準点へ
///        毎フレーム追従させるコンポーネント
/// @details SpriteRenderer/TextRendererの矩形をどこに置くかには関与しない（それらが持つ
///          anchor_/pivot_はスプライト/テキスト自身の矩形内の基準点で、これとは別の概念）。
///          このコンポーネントは単にTransformのX/Y（Zは変更しない）を
///          「anchorPoint × Camera2Dの現在のwidth/height + offset」へ書き換えるだけの、
///          UIButton/MeshButtonと同系統の単機能コンポーネント。
///          対象のCamera2D側でAutoSyncSizeを有効にしておけば、描画先の解像度変化にも連動する
///          （Camera2D::SetAutoSyncSize参照。ScreenAnchor自身は描画先を一切問い合わせない）。
///          anchorPointは(0,0)=画面左下、(1,1)=画面右上（Camera2Dのワールド空間そのままの向き。
///          Y軸はワールド座標と同じく上方向が正）。
class ScreenAnchor final : public IObjectComponent {
public:
    OBJECT_COMPONENT_CONSTRUCTOR(ScreenAnchor, 0xFF, )
    COMPONENT_CATEGORY("Render")
    ~ScreenAnchor() override = default;

    std::unique_ptr<IObjectComponent> Clone() const override {
        auto ptr = std::make_unique<ScreenAnchor>();
        ptr->cameraObjectID_ = cameraObjectID_;
        ptr->anchorPoint_ = anchorPoint_;
        ptr->offset_ = offset_;
        return ptr;
    }

    //==================================================
    // 参照設定
    //==================================================

    /// @brief 基準にするCamera2Dを持つオブジェクトを設定する
    void SetCameraObject(const EmptyObject *cameraObject) {
        cameraObjectID_ = cameraObject ? cameraObject->GetObjectID() : UUID128();
    }
    void SetCameraObject(const UUID128 &cameraObjectID) { cameraObjectID_ = cameraObjectID; }
    const UUID128 &GetCameraObjectID() const noexcept { return cameraObjectID_; }
    EmptyObject *GetCameraObject() const {
        auto *sceneContext = GetOwnerSceneContext();
        if (!sceneContext || !cameraObjectID_.IsValid()) return nullptr;
        return sceneContext->GetSceneObject(cameraObjectID_);
    }

    /// @brief 画面上の基準点を設定する（(0,0)=画面左下 ～ (1,1)=画面右上の正規化座標）
    void SetAnchorPoint(const Vector2 &anchorPoint) { anchorPoint_ = anchorPoint; }
    const Vector2 &GetAnchorPoint() const noexcept { return anchorPoint_; }
    /// @brief 基準点からのオフセット（ピクセル、Camera2Dのワールド単位＝Y上方向が正）を設定する
    void SetOffset(const Vector2 &offset) { offset_ = offset; }
    const Vector2 &GetOffset() const noexcept { return offset_; }

protected:
    void Update() override {
        auto *cameraObj = GetCameraObject();
        auto *camera2d = cameraObj ? cameraObj->GetComponent<Camera2D>() : nullptr;
        if (!camera2d) return;

        auto *objectContext = GetOwnerObjectContext();
        auto *transform = objectContext ? objectContext->GetComponent<Transform>() : nullptr;
        if (!transform) return;

        Vector3 translate = transform->GetTranslate();
        translate.x = anchorPoint_.x * camera2d->GetWidth() + offset_.x;
        translate.y = anchorPoint_.y * camera2d->GetHeight() + offset_.y;
        transform->SetTranslate(translate);
    }

#if defined(USE_IMGUI)
    void ShowImGui() override {
        TargetObjectSelector::ShowSelector(TranslationLabel("component.screenanchor.camera"), GetOwnerSceneContext(), cameraObjectID_, true, false);
        ImGui::DragFloat2(TranslationLabel("component.screenanchor.anchor_point"), &anchorPoint_.x, 0.01f, 0.0f, 1.0f);
        ImGui::DragFloat2(TranslationLabel("component.screenanchor.offset"), &offset_.x, 1.0f);
        ImGui::TextDisabled("%s", TranslationC("component.screenanchor.desc"));
    }
#endif

    JSON SaveToJson() const override {
        JSON json = JSON::object();
        json["cameraObjectID"] = ToJSON(cameraObjectID_);
        json["anchorPoint"] = ToJSON(anchorPoint_);
        json["offset"] = ToJSON(offset_);
        return json;
    }
    bool LoadFromJson(const JSON &json) override {
        cameraObjectID_ = json.contains("cameraObjectID") ? FromJSON<UUID128>(json["cameraObjectID"]) : UUID128();
        anchorPoint_ = json.contains("anchorPoint") ? FromJSON<Vector2>(json["anchorPoint"]) : Vector2(0.5f, 0.5f);
        offset_ = json.contains("offset") ? FromJSON<Vector2>(json["offset"]) : Vector2(0.0f, 0.0f);
        return true;
    }

private:
    UUID128 cameraObjectID_{};
    Vector2 anchorPoint_{ 0.5f, 0.5f };
    Vector2 offset_{ 0.0f, 0.0f };
};

REGISTER_COMPONENT_OBJECT(ScreenAnchor)

} // namespace KashipanEngine
