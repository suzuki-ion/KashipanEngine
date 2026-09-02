#pragma once
#include <algorithm>
#include <vector>

#include "Objects/ObjectComponentHeader.h"
#include "Objects/Components/Render/Camera2D.h"
#include "Objects/Components/Transform.h"
#include "Math/Quaternion.h"
#include "Math/Vector2.h"
#include "Math/Vector3.h"
#include "Utilities/MathUtils.h"
#include "Utilities/TimeUtils.h"
#include "Utilities/UUID128.h"
#if defined(USE_IMGUI)
#include "Objects/Components/Render/TargetObjectSelector.h"
#include "Utilities/Translation.h"
#endif

namespace KashipanEngine {

/// @brief 同一オブジェクトの Camera2D を、複数の追従先オブジェクトへ滑らかに追従させるコンポーネント
/// @details CameraController（3D用）の2D版。追従に用いるのは位置X/Y・回転Z（画面内の回転）のみで、
///          FovYの代わりにCamera2Dのwidth/height（表示範囲＝ズーム相当）を目標値へ遷移させる。
///          追従先オブジェクトは複数指定可能で、それぞれ移動(x,y)・回転(z)のどちらを追従に
///          用いるかを個別に選択できる（複数の追従先が同じ軸を指定した場合は平均を取る）。
///          追従先オブジェクトが1つも指定されていない場合は、このコンポーネントが付与された
///          オブジェクト自身のTransformを追従先として扱う。
class CameraController2D final : public IObjectComponent {
public:
    OBJECT_COMPONENT_CONSTRUCTOR(CameraController2D, 1,
        ADD_MEMBER_VARIABLE(positionOffset_);
        ADD_MEMBER_VARIABLE(rotationOffset_);
        ADD_MEMBER_VARIABLE(targetSize_);
        ADD_MEMBER_VARIABLE(sizeLerpFactor_);
        ADD_MEMBER_VARIABLE(moveLerpFactor_.usePerAxis);
        ADD_MEMBER_VARIABLE(moveLerpFactor_.all);
        ADD_MEMBER_VARIABLE(moveLerpFactor_.perAxis);
        ADD_MEMBER_VARIABLE(rotateLerpFactor_);
    )
    COMPONENT_CATEGORY("Render")
    ~CameraController2D() override = default;

    std::unique_ptr<IObjectComponent> Clone() const override {
        auto ptr = std::make_unique<CameraController2D>();
        ptr->followTargets_ = followTargets_;
        ptr->positionOffset_ = positionOffset_;
        ptr->rotationOffset_ = rotationOffset_;
        ptr->targetSize_ = targetSize_;
        ptr->moveLerpFactor_ = moveLerpFactor_;
        ptr->rotateLerpFactor_ = rotateLerpFactor_;
        ptr->sizeLerpFactor_ = sizeLerpFactor_;
        return ptr;
    }

    /// @brief 追従先オブジェクト1件分の設定
    struct FollowTarget {
        UUID128 objectID{};
        bool followPositionX = true;
        bool followPositionY = true;
        bool followRotationZ = false;
    };

    /// @brief 軸ごとの移動追従の強さ（all=全軸共通の値を使う、per-axis=軸ごとに個別の値を使う）
    struct AxisStrength2 {
        bool usePerAxis = false;
        float all = 0.1f;
        Vector2 perAxis{ 0.1f, 0.1f };

        float GetX() const noexcept { return usePerAxis ? perAxis.x : all; }
        float GetY() const noexcept { return usePerAxis ? perAxis.y : all; }
    };

    /// @brief 同一オブジェクトから Camera2D コンポーネントが取得できるか（制御可能かどうか）
    bool IsControllable() const {
        auto *objectContext = GetOwnerObjectContext();
        return objectContext && objectContext->GetComponent<Camera2D>() != nullptr;
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

    void SetPositionOffset(const Vector2 &offset) { positionOffset_ = offset; }
    const Vector2 &GetPositionOffset() const noexcept { return positionOffset_; }
    /// @brief 回転オフセット（ラジアン、画面内回転＝Z軸のみ）
    void SetRotationOffset(float offset) { rotationOffset_ = offset; }
    float GetRotationOffset() const noexcept { return rotationOffset_; }
    /// @brief Camera2Dの目標width/height（ズーム相当）。対象のCamera2DでAutoSyncSizeが
    ///        有効な場合は毎フレーム描画先解像度へ上書きされるため実質無効になる点に注意
    void SetTargetSize(const Vector2 &size) { targetSize_ = size; }
    const Vector2 &GetTargetSize() const noexcept { return targetSize_; }

    //==================================================
    // 追従の強さ
    //==================================================

    AxisStrength2 &GetMoveStrength() noexcept { return moveLerpFactor_; }
    const AxisStrength2 &GetMoveStrength() const noexcept { return moveLerpFactor_; }
    void SetRotateLerpFactor(float factor) { rotateLerpFactor_ = factor; }
    float GetRotateLerpFactor() const noexcept { return rotateLerpFactor_; }
    void SetSizeLerpFactor(float factor) { sizeLerpFactor_ = factor; }
    float GetSizeLerpFactor() const noexcept { return sizeLerpFactor_; }

protected:
    void Update() override {
        auto *objectContext = GetOwnerObjectContext();
        if (!objectContext) return;
        auto *camera = objectContext->GetComponent<Camera2D>();
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

        Vector2 posSum{ 0.0f, 0.0f };
        int posCountX = 0, posCountY = 0;
        float rotSum = 0.0f;
        int rotCount = 0;

        for (const auto &followTarget : *targets) {
            EmptyObject *targetObject = sceneContext ? sceneContext->GetSceneObject(followTarget.objectID) : nullptr;
            if (!targetObject) continue;
            auto *targetTransform = targetObject->GetComponent<Transform>();
            if (!targetTransform) continue;

            if (followTarget.followPositionX || followTarget.followPositionY) {
                const Matrix4x4 &worldMatrix = targetTransform->GetWorldMatrix();
                if (followTarget.followPositionX) { posSum.x += worldMatrix.m[3][0]; ++posCountX; }
                if (followTarget.followPositionY) { posSum.y += worldMatrix.m[3][1]; ++posCountY; }
            }
            if (followTarget.followRotationZ) {
                Quaternion worldRotation = GetWorldRotationQuaternion(targetTransform);
                rotSum += worldRotation.MakeEuler().z;
                ++rotCount;
            }
        }

        // 追従先が寄与しない軸は現在値を維持する（オフセットも寄与がある軸にのみ適用する）
        const Vector3 currentTranslate = cameraTransform->GetTranslate();
        Vector3 desiredTranslate = currentTranslate;
        if (posCountX > 0) desiredTranslate.x = posSum.x / static_cast<float>(posCountX) + positionOffset_.x;
        if (posCountY > 0) desiredTranslate.y = posSum.y / static_cast<float>(posCountY) + positionOffset_.y;

        const Vector3 currentEuler = cameraTransform->GetRotate();
        float desiredRotZ = currentEuler.z;
        if (rotCount > 0) desiredRotZ = rotSum / static_cast<float>(rotCount) + rotationOffset_;

        const float dt = std::max(0.0f, GetDeltaTime() * GetGameSpeed());
        auto axisLerpT = [dt](float factor) { return std::clamp(factor * dt * 60.0f, 0.0f, 1.0f); };

        Vector3 newTranslate = currentTranslate;
        newTranslate.x = currentTranslate.x + (desiredTranslate.x - currentTranslate.x) * axisLerpT(moveLerpFactor_.GetX());
        newTranslate.y = currentTranslate.y + (desiredTranslate.y - currentTranslate.y) * axisLerpT(moveLerpFactor_.GetY());
        cameraTransform->SetTranslate(newTranslate);

        Vector3 newEuler = currentEuler;
        newEuler.z = LerpAngle(currentEuler.z, desiredRotZ, axisLerpT(rotateLerpFactor_));
        cameraTransform->SetRotate(newEuler);

        const float currentWidth = camera->GetWidth();
        const float currentHeight = camera->GetHeight();
        const float sizeT = axisLerpT(sizeLerpFactor_);
        camera->SetSize(
            currentWidth + (targetSize_.x - currentWidth) * sizeT,
            currentHeight + (targetSize_.y - currentHeight) * sizeT
        );
    }

#if defined(USE_IMGUI)
    void ShowImGui() override {
        ImGui::TextColored(IsControllable() ? ImVec4(0.5f, 1.0f, 0.5f, 1.0f) : ImVec4(1.0f, 0.5f, 0.5f, 1.0f),
            IsControllable() ? "Controllable (Camera2D found)" : "Not controllable (Camera2D component required)");
        ImGui::Separator();

        ImGui::TextUnformatted(TranslationC("component.cameracontroller2d.follow_targets"));
        int removeIndex = -1;
        for (size_t i = 0; i < followTargets_.size(); ++i) {
            ImGui::PushID(static_cast<int>(i));
            auto &target = followTargets_[i];
            TargetObjectSelector::ShowSelector(TranslationLabel("component.common.target"), GetOwnerSceneContext(), target.objectID, true, false);
            ImGui::TextUnformatted(TranslationC("component.cameracontroller2d.follow_position"));
            ImGui::Checkbox("X##Pos", &target.followPositionX); ImGui::SameLine();
            ImGui::Checkbox("Y##Pos", &target.followPositionY);
            ImGui::Checkbox(TranslationLabel("component.cameracontroller2d.follow_rotation"), &target.followRotationZ);
            if (ImGui::Button(TranslationLabel("component.cameracontroller2d.remove_target"))) removeIndex = static_cast<int>(i);
            ImGui::Separator();
            ImGui::PopID();
        }
        if (removeIndex >= 0) RemoveFollowTarget(static_cast<size_t>(removeIndex));
        if (ImGui::Button(TranslationLabel("component.cameracontroller2d.add_follow_target"))) {
            followTargets_.push_back(FollowTarget{});
        }
        if (followTargets_.empty()) {
            ImGui::TextDisabled("%s", TranslationC("component.cameracontroller2d.no_target_following_own_transform"));
        }

        ImGui::Separator();
        ImGui::DragFloat2(TranslationLabel("component.cameracontroller2d.position_offset"), &positionOffset_.x, 0.01f);
        float rotationOffsetDeg = ToDegrees(rotationOffset_);
        if (ImGui::DragFloat(TranslationLabel("component.cameracontroller2d.rotation_offset"), &rotationOffsetDeg, 0.1f)) {
            rotationOffset_ = ToRadians(rotationOffsetDeg);
        }
        ImGui::DragFloat2(TranslationLabel("component.cameracontroller2d.target_size"), &targetSize_.x, 1.0f);

        ImGui::Separator();
        ImGui::TextUnformatted(TranslationC("component.cameracontroller2d.move_follow_strength"));
        ImGui::Checkbox((std::string(TranslationC("component.cameracontroller2d.move_per_axis")) + "##Move").c_str(), &moveLerpFactor_.usePerAxis);
        if (moveLerpFactor_.usePerAxis) {
            ImGui::DragFloat2(TranslationLabel("component.cameracontroller2d.move_x_y"), &moveLerpFactor_.perAxis.x, 0.01f, 0.0f, 1.0f);
        } else {
            ImGui::DragFloat(TranslationLabel("component.cameracontroller2d.move_all"), &moveLerpFactor_.all, 0.01f, 0.0f, 1.0f);
        }
        ImGui::DragFloat(TranslationLabel("component.cameracontroller2d.rotate_follow_strength"), &rotateLerpFactor_, 0.01f, 0.0f, 1.0f);

        ImGui::Separator();
        ImGui::DragFloat(TranslationLabel("component.cameracontroller2d.size_transition_strength"), &sizeLerpFactor_, 0.01f, 0.0f, 1.0f);
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
            t["followRotationZ"] = target.followRotationZ;
            targetsJson.push_back(t);
        }
        json["followTargets"] = targetsJson;

        json["positionOffset"] = ToJSON(positionOffset_);
        json["rotationOffset"] = rotationOffset_;
        json["targetSize"] = ToJSON(targetSize_);

        json["moveLerpUsePerAxis"] = moveLerpFactor_.usePerAxis;
        json["moveLerpAll"] = moveLerpFactor_.all;
        json["moveLerpPerAxis"] = ToJSON(moveLerpFactor_.perAxis);

        json["rotateLerpFactor"] = rotateLerpFactor_;
        json["sizeLerpFactor"] = sizeLerpFactor_;
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
                target.followRotationZ = t.value("followRotationZ", false);
                followTargets_.push_back(target);
            }
        }

        if (json.contains("positionOffset")) positionOffset_ = FromJSON<Vector2>(json["positionOffset"]);
        rotationOffset_ = json.value("rotationOffset", 0.0f);
        if (json.contains("targetSize")) {
            targetSize_ = FromJSON<Vector2>(json["targetSize"]);
        } else {
            targetSize_ = Vector2(1280.0f, 720.0f);
        }

        moveLerpFactor_.usePerAxis = json.value("moveLerpUsePerAxis", false);
        moveLerpFactor_.all = json.value("moveLerpAll", 0.1f);
        if (json.contains("moveLerpPerAxis")) moveLerpFactor_.perAxis = FromJSON<Vector2>(json["moveLerpPerAxis"]);

        rotateLerpFactor_ = json.value("rotateLerpFactor", 0.1f);
        sizeLerpFactor_ = json.value("sizeLerpFactor", 0.1f);
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

    Vector2 positionOffset_{ 0.0f, 0.0f };
    /// @brief 回転オフセット（ラジアン、画面内回転＝Z軸のみ）
    float rotationOffset_ = 0.0f;
    /// @brief 目標のwidth/height（ズーム相当）。既定はCamera2Dの既定値と同じ
    Vector2 targetSize_{ 1280.0f, 720.0f };

    AxisStrength2 moveLerpFactor_{};
    float rotateLerpFactor_ = 0.1f;
    float sizeLerpFactor_ = 0.1f;
};

REGISTER_COMPONENT_OBJECT(CameraController2D)

} // namespace KashipanEngine
