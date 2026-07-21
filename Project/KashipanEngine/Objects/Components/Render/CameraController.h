#pragma once
#include <algorithm>
#include <vector>

#include "Objects/ObjectComponentHeader.h"
#include "Objects/Components/Render/Camera3D.h"
#include "Objects/Components/Transform.h"
#include "Math/Quaternion.h"
#include "Math/Vector3.h"
#include "Utilities/MathUtils.h"
#include "Utilities/TimeUtils.h"
#include "Utilities/UUID128.h"
#if defined(USE_IMGUI)
#include "Objects/Components/Render/TargetObjectSelector.h"
#endif

namespace KashipanEngine {

/// @brief 同一オブジェクトの Camera3D を、複数の追従先オブジェクトへ滑らかに追従させるコンポーネント
/// @details 追従先オブジェクトは複数指定可能で、それぞれ移動(x,y,z)・回転(x,y,z)のどの軸を
///          追従に用いるかを個別に選択できる（複数の追従先が同じ軸を指定した場合は平均を取る）。
///          追従先オブジェクトが1つも指定されていない場合は、このコンポーネントが付与された
///          オブジェクト自身のTransformを追従先として扱う。
class CameraController final : public IObjectComponent {
public:
    OBJECT_COMPONENT_CONSTRUCTOR(CameraController, 1,
        ADD_MEMBER_VARIABLE(positionOffset_);
        ADD_MEMBER_VARIABLE(rotationOffset_);
        ADD_MEMBER_VARIABLE(targetFovY_);
        ADD_MEMBER_VARIABLE(fovLerpFactor_);
        ADD_MEMBER_VARIABLE(moveLerpFactor_.usePerAxis);
        ADD_MEMBER_VARIABLE(moveLerpFactor_.all);
        ADD_MEMBER_VARIABLE(moveLerpFactor_.perAxis);
        ADD_MEMBER_VARIABLE(rotateLerpFactor_.usePerAxis);
        ADD_MEMBER_VARIABLE(rotateLerpFactor_.all);
        ADD_MEMBER_VARIABLE(rotateLerpFactor_.perAxis);
    )
    COMPONENT_CATEGORY("Render")
    ~CameraController() override = default;

    std::unique_ptr<IObjectComponent> Clone() const override {
        auto ptr = std::make_unique<CameraController>();
        ptr->followTargets_ = followTargets_;
        ptr->positionOffset_ = positionOffset_;
        ptr->rotationOffset_ = rotationOffset_;
        ptr->targetFovY_ = targetFovY_;
        ptr->moveLerpFactor_ = moveLerpFactor_;
        ptr->rotateLerpFactor_ = rotateLerpFactor_;
        ptr->fovLerpFactor_ = fovLerpFactor_;
        return ptr;
    }

    /// @brief 追従先オブジェクト1件分の設定
    struct FollowTarget {
        UUID128 objectID{};
        bool followPositionX = true;
        bool followPositionY = true;
        bool followPositionZ = true;
        bool followRotationX = false;
        bool followRotationY = false;
        bool followRotationZ = false;
    };

    /// @brief 軸ごとの追従の強さ（all=全軸共通の値を使う、per-axis=軸ごとに個別の値を使う）
    struct AxisStrength {
        bool usePerAxis = false;
        float all = 0.1f;
        Vector3 perAxis{ 0.1f, 0.1f, 0.1f };

        float GetX() const noexcept { return usePerAxis ? perAxis.x : all; }
        float GetY() const noexcept { return usePerAxis ? perAxis.y : all; }
        float GetZ() const noexcept { return usePerAxis ? perAxis.z : all; }
    };

    /// @brief 同一オブジェクトから Camera3D コンポーネントが取得できるか（制御可能かどうか）
    bool IsControllable() const {
        auto *objectContext = GetOwnerObjectContext();
        return objectContext && objectContext->GetComponent<Camera3D>() != nullptr;
    }

    //==================================================
    // 追従先オブジェクト
    //==================================================

    std::vector<FollowTarget> &GetFollowTargets() noexcept { return followTargets_; }
    const std::vector<FollowTarget> &GetFollowTargets() const noexcept { return followTargets_; }
    void AddFollowTarget(const UUID128 &objectID) {
        FollowTarget target;
        target.objectID = objectID;
        followTargets_.push_back(target);
    }
    void RemoveFollowTarget(size_t index) {
        if (index < followTargets_.size()) followTargets_.erase(followTargets_.begin() + index);
    }

    //==================================================
    // オフセット・目標値
    //==================================================

    void SetPositionOffset(const Vector3 &offset) { positionOffset_ = offset; }
    const Vector3 &GetPositionOffset() const noexcept { return positionOffset_; }
    /// @brief 回転オフセット（オイラー角、ラジアン）
    void SetRotationOffset(const Vector3 &offset) { rotationOffset_ = offset; }
    const Vector3 &GetRotationOffset() const noexcept { return rotationOffset_; }
    void SetTargetFovY(float fovY) { targetFovY_ = fovY; }
    float GetTargetFovY() const noexcept { return targetFovY_; }

    //==================================================
    // 追従の強さ
    //==================================================

    AxisStrength &GetMoveStrength() noexcept { return moveLerpFactor_; }
    const AxisStrength &GetMoveStrength() const noexcept { return moveLerpFactor_; }
    AxisStrength &GetRotateStrength() noexcept { return rotateLerpFactor_; }
    const AxisStrength &GetRotateStrength() const noexcept { return rotateLerpFactor_; }
    void SetFovLerpFactor(float factor) { fovLerpFactor_ = factor; }
    float GetFovLerpFactor() const noexcept { return fovLerpFactor_; }

protected:
    void Update() override {
        auto *objectContext = GetOwnerObjectContext();
        if (!objectContext) return;
        auto *camera = objectContext->GetComponent<Camera3D>();
        if (!camera) return;
        auto *cameraTransform = objectContext->GetComponent<Transform>();
        if (!cameraTransform) return;

        auto *sceneContext = GetOwnerSceneContext();

        // 追従先が1つも指定されていない場合は自身のTransformを追従先として扱う
        std::vector<FollowTarget> fallbackTargets;
        const std::vector<FollowTarget> *targets = &followTargets_;
        if (followTargets_.empty()) {
            FollowTarget self;
            self.objectID = objectContext->GetObjectID();
            fallbackTargets.push_back(self);
            targets = &fallbackTargets;
        }

        Vector3 posSum{ 0.0f, 0.0f, 0.0f };
        int posCountX = 0, posCountY = 0, posCountZ = 0;
        Vector3 rotEulerSum{ 0.0f, 0.0f, 0.0f };
        int rotCountX = 0, rotCountY = 0, rotCountZ = 0;

        for (const auto &followTarget : *targets) {
            EmptyObject *targetObject = sceneContext ? sceneContext->GetSceneObject(followTarget.objectID) : nullptr;
            if (!targetObject) continue;
            auto *targetTransform = targetObject->GetComponent<Transform>();
            if (!targetTransform) continue;

            if (followTarget.followPositionX || followTarget.followPositionY || followTarget.followPositionZ) {
                const Matrix4x4 &worldMatrix = targetTransform->GetWorldMatrix();
                const Vector3 worldPosition(worldMatrix.m[3][0], worldMatrix.m[3][1], worldMatrix.m[3][2]);
                if (followTarget.followPositionX) { posSum.x += worldPosition.x; ++posCountX; }
                if (followTarget.followPositionY) { posSum.y += worldPosition.y; ++posCountY; }
                if (followTarget.followPositionZ) { posSum.z += worldPosition.z; ++posCountZ; }
            }
            if (followTarget.followRotationX || followTarget.followRotationY || followTarget.followRotationZ) {
                Quaternion worldRotation = GetWorldRotationQuaternion(targetTransform);
                const Vector3 worldEuler = worldRotation.MakeEuler();
                if (followTarget.followRotationX) { rotEulerSum.x += worldEuler.x; ++rotCountX; }
                if (followTarget.followRotationY) { rotEulerSum.y += worldEuler.y; ++rotCountY; }
                if (followTarget.followRotationZ) { rotEulerSum.z += worldEuler.z; ++rotCountZ; }
            }
        }

        // 追従先が寄与しない軸は現在値を維持する（オフセットも寄与がある軸にのみ適用する）
        const Vector3 currentTranslate = cameraTransform->GetTranslate();
        Vector3 desiredTranslate = currentTranslate;
        if (posCountX > 0) desiredTranslate.x = posSum.x / static_cast<float>(posCountX) + positionOffset_.x;
        if (posCountY > 0) desiredTranslate.y = posSum.y / static_cast<float>(posCountY) + positionOffset_.y;
        if (posCountZ > 0) desiredTranslate.z = posSum.z / static_cast<float>(posCountZ) + positionOffset_.z;

        const Vector3 currentEuler = cameraTransform->GetRotate();
        Vector3 desiredEuler = currentEuler;
        if (rotCountX > 0) desiredEuler.x = rotEulerSum.x / static_cast<float>(rotCountX) + rotationOffset_.x;
        if (rotCountY > 0) desiredEuler.y = rotEulerSum.y / static_cast<float>(rotCountY) + rotationOffset_.y;
        if (rotCountZ > 0) desiredEuler.z = rotEulerSum.z / static_cast<float>(rotCountZ) + rotationOffset_.z;

        const float dt = std::max(0.0f, GetDeltaTime() * GetGameSpeed());
        auto axisLerpT = [dt](float factor) { return std::clamp(factor * dt * 60.0f, 0.0f, 1.0f); };

        Vector3 newTranslate;
        newTranslate.x = currentTranslate.x + (desiredTranslate.x - currentTranslate.x) * axisLerpT(moveLerpFactor_.GetX());
        newTranslate.y = currentTranslate.y + (desiredTranslate.y - currentTranslate.y) * axisLerpT(moveLerpFactor_.GetY());
        newTranslate.z = currentTranslate.z + (desiredTranslate.z - currentTranslate.z) * axisLerpT(moveLerpFactor_.GetZ());
        cameraTransform->SetTranslate(newTranslate);

        Vector3 newEuler;
        newEuler.x = LerpAngle(currentEuler.x, desiredEuler.x, axisLerpT(rotateLerpFactor_.GetX()));
        newEuler.y = LerpAngle(currentEuler.y, desiredEuler.y, axisLerpT(rotateLerpFactor_.GetY()));
        newEuler.z = LerpAngle(currentEuler.z, desiredEuler.z, axisLerpT(rotateLerpFactor_.GetZ()));
        cameraTransform->SetRotate(newEuler);

        const float currentFov = camera->GetFovY();
        camera->SetFovY(currentFov + (targetFovY_ - currentFov) * axisLerpT(fovLerpFactor_));
    }

#if defined(USE_IMGUI)
    void ShowImGui() override {
        ImGui::TextColored(IsControllable() ? ImVec4(0.5f, 1.0f, 0.5f, 1.0f) : ImVec4(1.0f, 0.5f, 0.5f, 1.0f),
            IsControllable() ? "Controllable (Camera3D found)" : "Not controllable (Camera3D component required)");
        ImGui::Separator();

        ImGui::TextUnformatted("Follow Targets");
        int removeIndex = -1;
        for (size_t i = 0; i < followTargets_.size(); ++i) {
            ImGui::PushID(static_cast<int>(i));
            auto &target = followTargets_[i];
            TargetObjectSelector::ShowSelector("Target", GetOwnerSceneContext(), target.objectID, true, false);
            ImGui::TextUnformatted("Follow Position");
            ImGui::Checkbox("X##Pos", &target.followPositionX); ImGui::SameLine();
            ImGui::Checkbox("Y##Pos", &target.followPositionY); ImGui::SameLine();
            ImGui::Checkbox("Z##Pos", &target.followPositionZ);
            ImGui::TextUnformatted("Follow Rotation");
            ImGui::Checkbox("X##Rot", &target.followRotationX); ImGui::SameLine();
            ImGui::Checkbox("Y##Rot", &target.followRotationY); ImGui::SameLine();
            ImGui::Checkbox("Z##Rot", &target.followRotationZ);
            if (ImGui::Button("Remove Target")) removeIndex = static_cast<int>(i);
            ImGui::Separator();
            ImGui::PopID();
        }
        if (removeIndex >= 0) RemoveFollowTarget(static_cast<size_t>(removeIndex));
        if (ImGui::Button("Add Follow Target")) {
            followTargets_.push_back(FollowTarget{});
        }
        if (followTargets_.empty()) {
            ImGui::TextDisabled("(No target: following own Transform)");
        }

        ImGui::Separator();
        ImGui::DragFloat3("Position Offset", &positionOffset_.x, 0.01f);
        Vector3 rotationOffsetDeg(ToDegrees(rotationOffset_.x), ToDegrees(rotationOffset_.y), ToDegrees(rotationOffset_.z));
        if (ImGui::DragFloat3("Rotation Offset", &rotationOffsetDeg.x, 0.1f)) {
            rotationOffset_ = Vector3(ToRadians(rotationOffsetDeg.x), ToRadians(rotationOffsetDeg.y), ToRadians(rotationOffsetDeg.z));
        }
        ImGui::DragFloat("Target FovY", &targetFovY_, 0.01f, 0.01f, GetPI<float>() - 0.01f);

        ImGui::Separator();
        ImGui::TextUnformatted("Move Follow Strength");
        ImGui::Checkbox("Move: Per-Axis##Move", &moveLerpFactor_.usePerAxis);
        if (moveLerpFactor_.usePerAxis) {
            ImGui::DragFloat3("Move X/Y/Z", &moveLerpFactor_.perAxis.x, 0.01f, 0.0f, 1.0f);
        } else {
            ImGui::DragFloat("Move All", &moveLerpFactor_.all, 0.01f, 0.0f, 1.0f);
        }

        ImGui::TextUnformatted("Rotate Follow Strength");
        ImGui::Checkbox("Rotate: Per-Axis##Rotate", &rotateLerpFactor_.usePerAxis);
        if (rotateLerpFactor_.usePerAxis) {
            ImGui::DragFloat3("Rotate X/Y/Z", &rotateLerpFactor_.perAxis.x, 0.01f, 0.0f, 1.0f);
        } else {
            ImGui::DragFloat("Rotate All", &rotateLerpFactor_.all, 0.01f, 0.0f, 1.0f);
        }

        ImGui::Separator();
        ImGui::DragFloat("Fov Transition Strength", &fovLerpFactor_, 0.01f, 0.0f, 1.0f);
    }
#endif

    JSON SaveToJson() const override {
        JSON json = JSON::object();

        JSON targetsJson = JSON::array();
        for (const auto &target : followTargets_) {
            JSON t = JSON::object();
            t["objectID"] = ToJSON(target.objectID);
            t["followPositionX"] = target.followPositionX;
            t["followPositionY"] = target.followPositionY;
            t["followPositionZ"] = target.followPositionZ;
            t["followRotationX"] = target.followRotationX;
            t["followRotationY"] = target.followRotationY;
            t["followRotationZ"] = target.followRotationZ;
            targetsJson.push_back(t);
        }
        json["followTargets"] = targetsJson;

        json["positionOffset"] = ToJSON(positionOffset_);
        json["rotationOffset"] = ToJSON(rotationOffset_);
        json["targetFovY"] = targetFovY_;

        json["moveLerpUsePerAxis"] = moveLerpFactor_.usePerAxis;
        json["moveLerpAll"] = moveLerpFactor_.all;
        json["moveLerpPerAxis"] = ToJSON(moveLerpFactor_.perAxis);

        json["rotateLerpUsePerAxis"] = rotateLerpFactor_.usePerAxis;
        json["rotateLerpAll"] = rotateLerpFactor_.all;
        json["rotateLerpPerAxis"] = ToJSON(rotateLerpFactor_.perAxis);

        json["fovLerpFactor"] = fovLerpFactor_;
        return json;
    }

    bool LoadFromJson(const JSON &json) override {
        followTargets_.clear();
        if (json.contains("followTargets")) {
            for (const auto &t : json["followTargets"]) {
                FollowTarget target;
                if (t.contains("objectID")) target.objectID = FromJSON<UUID128>(t["objectID"]);
                target.followPositionX = t.value("followPositionX", true);
                target.followPositionY = t.value("followPositionY", true);
                target.followPositionZ = t.value("followPositionZ", true);
                target.followRotationX = t.value("followRotationX", false);
                target.followRotationY = t.value("followRotationY", false);
                target.followRotationZ = t.value("followRotationZ", false);
                followTargets_.push_back(target);
            }
        }

        if (json.contains("positionOffset")) positionOffset_ = FromJSON<Vector3>(json["positionOffset"]);
        if (json.contains("rotationOffset")) rotationOffset_ = FromJSON<Vector3>(json["rotationOffset"]);
        targetFovY_ = json.value("targetFovY", 0.45f);

        moveLerpFactor_.usePerAxis = json.value("moveLerpUsePerAxis", false);
        moveLerpFactor_.all = json.value("moveLerpAll", 0.1f);
        if (json.contains("moveLerpPerAxis")) moveLerpFactor_.perAxis = FromJSON<Vector3>(json["moveLerpPerAxis"]);

        rotateLerpFactor_.usePerAxis = json.value("rotateLerpUsePerAxis", false);
        rotateLerpFactor_.all = json.value("rotateLerpAll", 0.1f);
        if (json.contains("rotateLerpPerAxis")) rotateLerpFactor_.perAxis = FromJSON<Vector3>(json["rotateLerpPerAxis"]);

        fovLerpFactor_ = json.value("fovLerpFactor", 0.1f);
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

    /// @brief 角度（ラジアン）を最短経路で補間する
    static float LerpAngle(float current, float target, float t) {
        constexpr float kPi = GetPI<float>();
        float delta = target - current;
        while (delta > kPi) delta -= 2.0f * kPi;
        while (delta < -kPi) delta += 2.0f * kPi;
        return current + delta * t;
    }

    std::vector<FollowTarget> followTargets_;

    Vector3 positionOffset_{ 0.0f, 0.0f, 0.0f };
    /// @brief 回転オフセット（オイラー角、ラジアン）
    Vector3 rotationOffset_{ 0.0f, 0.0f, 0.0f };
    float targetFovY_ = 0.45f;

    AxisStrength moveLerpFactor_{};
    AxisStrength rotateLerpFactor_{};
    float fovLerpFactor_ = 0.1f;
};

REGISTER_COMPONENT_OBJECT(CameraController)

} // namespace KashipanEngine
