#pragma once
#include <cmath>

#include "Objects/ObjectComponentHeader.h"
#include "Objects/Components/Transform.h"
#include "Math/Quaternion.h"
#include "Math/Vector3.h"
#include "Utilities/MathUtils.h"
#include "Utilities/UUID128.h"
#if defined(USE_IMGUI)
#include "Objects/Components/Render/TargetObjectSelector.h"
#endif

namespace KashipanEngine {

/// @brief 指定オブジェクトへ向き続ける（ビルボードのような動作をする）コンポーネント
/// @details 回転の決め方は2種類から選択できる。
///          - SyncRotation: ターゲットのワールド回転と自身の回転を同期する
///            （カメラをターゲットにすると、常に画面と正対するビルボードになる）
///          - LookAt: 自身の+Z軸が常にターゲットの方向を向くよう回転する
///          どちらのモードでも回転オフセット（オイラー角）を追加で適用できる。
class TargetLookAt final : public IObjectComponent {
public:
    OBJECT_COMPONENT_CONSTRUCTOR(TargetLookAt, 1, )
    COMPONENT_CATEGORY("Transform")
    ~TargetLookAt() override = default;

    /// @brief 回転の決定方法
    enum class RotationMode {
        SyncRotation = 0, ///< ターゲットのワールド回転と同期する（ビルボード向け）
        LookAt = 1,       ///< ターゲットの方向（+Z軸がターゲットを向く）に回転する
    };

    std::unique_ptr<IObjectComponent> Clone() const override {
        auto ptr = std::make_unique<TargetLookAt>();
        ptr->targetObjectID_ = targetObjectID_;
        ptr->rotationOffset_ = rotationOffset_;
        ptr->rotationMode_ = rotationMode_;
        return ptr;
    }

    //==================================================
    // ターゲットオブジェクト
    //==================================================

    /// @brief ターゲットオブジェクトを設定する
    void SetTargetObject(const EmptyObject *targetObject) {
        targetObjectID_ = targetObject ? targetObject->GetObjectID() : UUID128();
    }
    /// @brief ターゲットオブジェクトをUUIDから設定する
    void SetTargetObject(const UUID128 &targetObjectID) { targetObjectID_ = targetObjectID; }
    /// @brief ターゲットオブジェクトのUUIDを取得する
    const UUID128 &GetTargetObjectID() const noexcept { return targetObjectID_; }
    /// @brief ターゲットオブジェクトを取得する（存在しない場合は nullptr）
    EmptyObject *GetTargetObject() const {
        auto *sceneContext = GetOwnerSceneContext();
        if (!sceneContext || !targetObjectID_.IsValid()) return nullptr;
        return sceneContext->GetSceneObject(targetObjectID_);
    }

    //==================================================
    // 回転設定
    //==================================================

    /// @brief 回転オフセット（オイラー角、ラジアン）を設定する
    void SetRotationOffset(const Vector3 &offset) { rotationOffset_ = offset; }
    const Vector3 &GetRotationOffset() const noexcept { return rotationOffset_; }
    /// @brief 回転の決定方法を設定する
    void SetRotationMode(RotationMode mode) noexcept { rotationMode_ = mode; }
    RotationMode GetRotationMode() const noexcept { return rotationMode_; }

protected:
    void Update() override {
        auto *objectContext = GetOwnerObjectContext();
        if (!objectContext) return;
        auto *transform = objectContext->GetComponent<Transform>();
        if (!transform) return;

        EmptyObject *targetObject = GetTargetObject();
        if (!targetObject) return;
        auto *targetTransform = targetObject->GetComponent<Transform>();
        if (!targetTransform) return;

        Quaternion desiredWorld;
        if (rotationMode_ == RotationMode::SyncRotation) {
            desiredWorld = GetWorldRotationQuaternion(targetTransform);
        } else {
            // ワールド行列の平行移動成分から自身→ターゲットの方向を求める
            const Matrix4x4 &selfWorld = transform->GetWorldMatrix();
            const Matrix4x4 &targetWorld = targetTransform->GetWorldMatrix();
            const Vector3 selfPos(selfWorld.m[3][0], selfWorld.m[3][1], selfWorld.m[3][2]);
            const Vector3 targetPos(targetWorld.m[3][0], targetWorld.m[3][1], targetWorld.m[3][2]);
            const Vector3 direction = targetPos - selfPos;
            if (direction.LengthSquared() <= 1.0e-12f) return;
            desiredWorld = MakeLookRotation(direction.Normalize());
        }
        desiredWorld = desiredWorld * Quaternion::MakeRotateEuler(rotationOffset_);

        // ワールド回転をローカル回転へ変換して適用する（world = parent * local）
        auto *parentObject = transform->GetParentObject();
        auto *parentTransform = parentObject ? parentObject->GetComponent<Transform>() : nullptr;
        if (parentTransform) {
            const Quaternion parentWorld = GetWorldRotationQuaternion(parentTransform);
            transform->SetRotateQuaternion((parentWorld.Inverse() * desiredWorld).Normalize());
        } else {
            transform->SetRotateQuaternion(desiredWorld.Normalize());
        }
    }

#if defined(USE_IMGUI)
    void ShowImGui() override {
        TargetObjectSelector::ShowSelector("Target", GetOwnerSceneContext(), targetObjectID_, true, false);
        if (!targetObjectID_.IsValid()) {
            ImGui::TextDisabled("(No target: rotation is not controlled)");
        }

        static const char *kModeLabels[] = { "Sync Target Rotation", "Look At Target" };
        int mode = static_cast<int>(rotationMode_);
        if (ImGui::Combo("Rotation Mode", &mode, kModeLabels, 2)) {
            rotationMode_ = static_cast<RotationMode>(mode);
        }
        if (ImGui::IsItemHovered()) {
            ImGui::SetTooltip("Sync Target Rotation: ターゲットのワールド回転と同期する（ビルボード向け）\nLook At Target: 自身の+Z軸が常にターゲットの方向を向く");
        }

        Vector3 rotationOffsetDeg(ToDegrees(rotationOffset_.x), ToDegrees(rotationOffset_.y), ToDegrees(rotationOffset_.z));
        if (ImGui::DragFloat3("Rotation Offset", &rotationOffsetDeg.x, 0.1f)) {
            rotationOffset_ = Vector3(ToRadians(rotationOffsetDeg.x), ToRadians(rotationOffsetDeg.y), ToRadians(rotationOffsetDeg.z));
        }
    }
#endif

    JSON SaveToJson() const override {
        JSON json = JSON::object();
        json["targetObjectID"] = ToJSON(targetObjectID_);
        json["rotationOffset"] = ToJSON(rotationOffset_);
        json["rotationMode"] = static_cast<int>(rotationMode_);
        return json;
    }

    bool LoadFromJson(const JSON &json) override {
        if (json.contains("targetObjectID")) targetObjectID_ = FromJSON<UUID128>(json["targetObjectID"]);
        if (json.contains("rotationOffset")) rotationOffset_ = FromJSON<Vector3>(json["rotationOffset"]);
        rotationMode_ = static_cast<RotationMode>(json.value("rotationMode", 0));
        return true;
    }

private:
    /// @brief Transformの親子関係をたどり、ワールド回転（クォータニオン）を合成する
    static Quaternion GetWorldRotationQuaternion(Transform *transform) {
        if (!transform) return Quaternion::Identity();
        const Quaternion local = transform->GetRotateQuaternion();
        EmptyObject *parentObject = transform->GetParentObject();
        Transform *parentTransform = parentObject ? parentObject->GetComponent<Transform>() : nullptr;
        if (parentTransform) {
            return GetWorldRotationQuaternion(parentTransform) * local;
        }
        return local;
    }

    /// @brief +Z軸が指定方向を向く回転（クォータニオン）を生成する
    /// @param forward 向かせたい方向（正規化済みであること）
    static Quaternion MakeLookRotation(const Vector3 &forward) {
        // forwardがワールドYとほぼ平行な場合は代替の上方向を使う
        Vector3 up(0.0f, 1.0f, 0.0f);
        if (std::abs(forward.y) > 0.999f) {
            up = Vector3(0.0f, 0.0f, 1.0f);
        }
        const Vector3 right = up.Cross(forward).Normalize();
        const Vector3 realUp = forward.Cross(right);

        // 行ベクトル規約（v' = v * M）の回転行列を組み立てて変換する
        Matrix4x4 mat = Matrix4x4::Identity();
        mat.m[0][0] = right.x;  mat.m[0][1] = right.y;  mat.m[0][2] = right.z;
        mat.m[1][0] = realUp.x; mat.m[1][1] = realUp.y; mat.m[1][2] = realUp.z;
        mat.m[2][0] = forward.x; mat.m[2][1] = forward.y; mat.m[2][2] = forward.z;
        return Quaternion::MakeFromRotationMatrix(mat);
    }

    UUID128 targetObjectID_{};
    /// @brief 回転オフセット（オイラー角、ラジアン）
    Vector3 rotationOffset_{ 0.0f, 0.0f, 0.0f };
    RotationMode rotationMode_ = RotationMode::SyncRotation;
};

REGISTER_COMPONENT_OBJECT(TargetLookAt)

} // namespace KashipanEngine
