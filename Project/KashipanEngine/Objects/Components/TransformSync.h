#pragma once
#include "Objects/ObjectComponentHeader.h"
#include "Objects/Components/Transform.h"
#include "Math/Quaternion.h"
#include "Math/Vector3.h"
#include "Utilities/MathUtils.h"
#include "Utilities/UUID128.h"
#if defined(USE_IMGUI)
#include "Objects/Components/Render/TargetObjectSelector.h"
#include "Utilities/Translation.h"
#endif

namespace KashipanEngine {

/// @brief 他オブジェクトのTransform（位置・回転・スケール）を、親子付けせずに自身へ同期するコンポーネント
/// @details 毎フレーム、ターゲットオブジェクトのワールド位置・回転・スケールにそれぞれのオフセットを
///          加えた値を、自身の（親がいる場合は親のワールド行列で変換した）ローカルTransformへ
///          書き込む。Transform::SetParentObjectによる実際のシーン階層上の親子関係は変更しない。
///          同期する項目（位置・回転・スケール）はそれぞれ個別にチェックボックスで有効/無効を
///          切り替えられ、有効にした項目ごとにオフセット値を設定できる。
class TransformSync final : public IObjectComponent {
public:
    OBJECT_COMPONENT_CONSTRUCTOR(TransformSync, 0xFF,
        ADD_MEMBER_VARIABLE(syncPosition_);
        ADD_MEMBER_VARIABLE(syncRotation_);
        ADD_MEMBER_VARIABLE(syncScale_);
        ADD_MEMBER_VARIABLE(positionOffset_);
        ADD_MEMBER_VARIABLE(rotationOffset_);
        ADD_MEMBER_VARIABLE(scaleOffset_);
    )
    COMPONENT_CATEGORY("Utility")
    ~TransformSync() override = default;

    std::unique_ptr<IObjectComponent> Clone() const override {
        auto ptr = std::make_unique<TransformSync>();
        ptr->targetObjectID_ = targetObjectID_;
        ptr->syncPosition_ = syncPosition_;
        ptr->syncRotation_ = syncRotation_;
        ptr->syncScale_ = syncScale_;
        ptr->positionOffset_ = positionOffset_;
        ptr->rotationOffset_ = rotationOffset_;
        ptr->scaleOffset_ = scaleOffset_;
        return ptr;
    }

    //==================================================
    // ターゲットオブジェクト
    //==================================================

    /// @brief 同期元のターゲットオブジェクトを設定する
    void SetTargetObject(const EmptyObject *targetObject) {
        targetObjectID_ = targetObject ? targetObject->GetObjectID() : UUID128();
    }
    /// @brief 同期元のターゲットオブジェクトをUUIDから設定する
    void SetTargetObject(const UUID128 &targetObjectID) { targetObjectID_ = targetObjectID; }
    /// @brief 同期元のターゲットオブジェクトのUUIDを取得する
    const UUID128 &GetTargetObjectID() const noexcept { return targetObjectID_; }
    /// @brief 同期元のターゲットオブジェクトを取得する（存在しない場合は nullptr）
    EmptyObject *GetTargetObject() const {
        auto *sceneContext = GetOwnerSceneContext();
        if (!sceneContext || !targetObjectID_.IsValid()) return nullptr;
        return sceneContext->GetSceneObject(targetObjectID_);
    }

    //==================================================
    // 同期する項目・オフセット
    //==================================================

    void SetSyncPosition(bool enable) noexcept { syncPosition_ = enable; }
    bool GetSyncPosition() const noexcept { return syncPosition_; }
    void SetSyncRotation(bool enable) noexcept { syncRotation_ = enable; }
    bool GetSyncRotation() const noexcept { return syncRotation_; }
    void SetSyncScale(bool enable) noexcept { syncScale_ = enable; }
    bool GetSyncScale() const noexcept { return syncScale_; }

    /// @brief 位置オフセットを設定する（ターゲットのワールド座標に加算される）
    void SetPositionOffset(const Vector3 &offset) noexcept { positionOffset_ = offset; }
    const Vector3 &GetPositionOffset() const noexcept { return positionOffset_; }
    /// @brief 回転オフセットを設定する（オイラー角、ラジアン。ターゲットのワールド回転に加算される）
    void SetRotationOffset(const Vector3 &offset) noexcept { rotationOffset_ = offset; }
    const Vector3 &GetRotationOffset() const noexcept { return rotationOffset_; }
    /// @brief スケールオフセットを設定する（ターゲットのワールドスケールに加算される）
    void SetScaleOffset(const Vector3 &offset) noexcept { scaleOffset_ = offset; }
    const Vector3 &GetScaleOffset() const noexcept { return scaleOffset_; }

protected:
    void Update() override {
        if (!(syncPosition_ || syncRotation_ || syncScale_)) return;
        if (!targetObjectID_.IsValid()) return;

        auto *objectContext = GetOwnerObjectContext();
        if (!objectContext) return;
        auto *transform = objectContext->GetComponent<Transform>();
        if (!transform) return;

        EmptyObject *targetObject = GetTargetObject();
        // 自分自身をターゲットにした場合の無限フィードバックを避ける
        if (!targetObject || targetObject == objectContext->GetOwner()) return;
        auto *targetTransform = targetObject->GetComponent<Transform>();
        if (!targetTransform) return;

        auto *parentObject = transform->GetParentObject();
        auto *parentTransform = parentObject ? parentObject->GetComponent<Transform>() : nullptr;

        if (syncPosition_) {
            const Vector3 desiredWorldPos = targetTransform->GetWorldPosition() + positionOffset_;
            transform->SetTranslate(parentTransform
                ? desiredWorldPos.Transform(parentTransform->GetWorldMatrix().Inverse())
                : desiredWorldPos);
        }

        if (syncRotation_) {
            const Quaternion desiredWorldRot = (targetTransform->GetWorldRotateQuaternion() * Quaternion::MakeRotateEuler(rotationOffset_)).Normalize();
            if (parentTransform) {
                const Quaternion parentWorldRot = parentTransform->GetWorldRotateQuaternion();
                transform->SetRotateQuaternion((desiredWorldRot * parentWorldRot.Inverse()).Normalize());
            } else {
                transform->SetRotateQuaternion(desiredWorldRot);
            }
        }

        if (syncScale_) {
            const Vector3 desiredWorldScale = targetTransform->GetWorldScale() + scaleOffset_;
            if (parentTransform) {
                const Vector3 parentWorldScale = parentTransform->GetWorldScale();
                transform->SetScale(Vector3(
                    parentWorldScale.x != 0.0f ? desiredWorldScale.x / parentWorldScale.x : desiredWorldScale.x,
                    parentWorldScale.y != 0.0f ? desiredWorldScale.y / parentWorldScale.y : desiredWorldScale.y,
                    parentWorldScale.z != 0.0f ? desiredWorldScale.z / parentWorldScale.z : desiredWorldScale.z
                ));
            } else {
                transform->SetScale(desiredWorldScale);
            }
        }
    }

#if defined(USE_IMGUI)
    void ShowImGui() override {
        ImGui::TextWrapped("%s", TranslationC("component.transformsync.desc_1"));
        ImGui::Spacing();

        TargetObjectSelector::ShowSelector("Target", GetOwnerSceneContext(), targetObjectID_, true, false);
        if (!targetObjectID_.IsValid()) {
            ImGui::TextDisabled("%s", TranslationC("component.transformsync.no_target"));
        }

        ImGui::Spacing();
        ImGui::Separator();

        ImGui::Checkbox(TranslationLabel("component.transformsync.sync_position"), &syncPosition_);
        if (syncPosition_) {
            ImGui::Indent();
            ImGuiCustom::EditValue(TranslationLabel("component.transformsync.position_offset"), positionOffset_, { .vSpeed = 0.01f });
            ImGui::Unindent();
        }

        ImGui::Checkbox(TranslationLabel("component.transformsync.sync_rotation"), &syncRotation_);
        if (syncRotation_) {
            ImGui::Indent();
            Vector3 rotationOffsetDeg(ToDegrees(rotationOffset_.x), ToDegrees(rotationOffset_.y), ToDegrees(rotationOffset_.z));
            if (ImGui::DragFloat3(TranslationLabel("component.transformsync.rotation_offset"), &rotationOffsetDeg.x, 0.1f)) {
                rotationOffset_ = Vector3(ToRadians(rotationOffsetDeg.x), ToRadians(rotationOffsetDeg.y), ToRadians(rotationOffsetDeg.z));
            }
            ImGui::Unindent();
        }

        ImGui::Checkbox(TranslationLabel("component.transformsync.sync_scale"), &syncScale_);
        if (syncScale_) {
            ImGui::Indent();
            ImGuiCustom::EditValue(TranslationLabel("component.transformsync.scale_offset"), scaleOffset_, { .vSpeed = 0.01f });
            ImGui::Unindent();
        }
    }
#endif

    JSON SaveToJson() const override {
        JSON json = JSON::object();
        json["targetObjectID"] = ToJSON(targetObjectID_);
        json["syncPosition"] = syncPosition_;
        json["syncRotation"] = syncRotation_;
        json["syncScale"] = syncScale_;
        json["positionOffset"] = ToJSON(positionOffset_);
        json["rotationOffset"] = ToJSON(rotationOffset_);
        json["scaleOffset"] = ToJSON(scaleOffset_);
        return json;
    }

    bool LoadFromJson(const JSON &json) override {
        if (json.contains("targetObjectID")) targetObjectID_ = FromJSON<UUID128>(json["targetObjectID"]);
        syncPosition_ = json.value("syncPosition", true);
        syncRotation_ = json.value("syncRotation", true);
        syncScale_ = json.value("syncScale", false);
        if (json.contains("positionOffset")) positionOffset_ = FromJSON<Vector3>(json["positionOffset"]);
        if (json.contains("rotationOffset")) rotationOffset_ = FromJSON<Vector3>(json["rotationOffset"]);
        if (json.contains("scaleOffset")) scaleOffset_ = FromJSON<Vector3>(json["scaleOffset"]);
        return true;
    }

private:
    UUID128 targetObjectID_{};

    bool syncPosition_ = true;
    bool syncRotation_ = true;
    bool syncScale_ = false;

    /// @brief 位置オフセット（ターゲットのワールド座標に加算する値）
    Vector3 positionOffset_{ 0.0f, 0.0f, 0.0f };
    /// @brief 回転オフセット（オイラー角、ラジアン。ターゲットのワールド回転に加算する値）
    Vector3 rotationOffset_{ 0.0f, 0.0f, 0.0f };
    /// @brief スケールオフセット（ターゲットのワールドスケールに加算する値）
    Vector3 scaleOffset_{ 0.0f, 0.0f, 0.0f };
};

REGISTER_COMPONENT_OBJECT(TransformSync)

} // namespace KashipanEngine
