#pragma once

#include <KashipanEngine.h>
#include "Scenes/Components/CameraController.h"
#include "Scenes/Components/StageGroundGenerator.h"
#include "Objects/Components/PlayerMovementController.h"
#include "Objects/Components/PlayerInputHandler.h"
#include "Utilities/FileIO/JSON.h"

#include <algorithm>
#include <cmath>
#include <filesystem>
#include <cstdio>

namespace KashipanEngine {

class CameraToPlayerSync final : public ISceneComponent {
public:
    explicit CameraToPlayerSync(Object3DBase *player)
        : ISceneComponent("CameraToPlayerSync", 1), player_(player) {}

    ~CameraToPlayerSync() override = default;

    void Initialize() override {
        if (loadParamsOnInitialize_) {
            (void)LoadParametersFromJson(paramJsonFilePath_);
        }
    }

    void SetClearViewEnabled(bool enabled) { clearViewEnabled_ = enabled; }

    void Update() override {
        if (!player_) return;

        auto *playerTr = player_->GetComponent3D<Transform3D>();
        auto *playerMovement = player_->GetComponent3D<PlayerMovementController>();
        auto *cameraController = GetOwnerContext() ? GetOwnerContext()->GetComponent<CameraController>() : nullptr;

        if (!playerTr || !playerMovement || !cameraController) return;

        const Vector3 playerPos = playerTr->GetTranslate();
        const Vector3 gravity = playerMovement->GetGravityDirection().Normalize();
        const Vector3 up = -gravity;
        const Vector3 forward = playerMovement->GetForwardDirection().Normalize();

        Vector3 right = up.Cross(forward);
        if (right.LengthSquared() <= 0.000001f) {
            right = Vector3{1.0f, 0.0f, 0.0f};
        } else {
            right = right.Normalize();
        }

        const float minSpeed = playerMovement->GetMinForwardSpeed();
        const float maxSpeed = playerMovement->GetMaxForwardSpeed();
        float speedRatio = 0.0f;
        if (maxSpeed > minSpeed) {
            speedRatio = (playerMovement->GetForwardSpeed() - minSpeed) / (maxSpeed - minSpeed);
        }
        speedRatio = std::clamp(speedRatio, 0.0f, 1.0f);

        const float followDistance = std::lerp(followDistanceMin_, followDistanceMax_, speedRatio);
        const float followHeight = std::lerp(followHeightMin_, followHeightMax_, speedRatio);
        const float lookAtHeight = std::lerp(lookAtHeightMin_, lookAtHeightMax_, speedRatio);

        Vector3 cameraPos = playerPos - forward * followDistance + up * followHeight;
        Vector3 lookDir = (playerPos + up * lookAtHeight - cameraPos).Normalize();

        if (clearViewEnabled_) {
            cameraPos = playerPos + right * clearViewRightDistance_ + forward * clearViewForwardDistance_ + up * clearViewHeight_;
            const Vector3 lookTarget = playerPos - right * clearViewLookOffsetRight_ + up * lookAtHeight;
            lookDir = (lookTarget - cameraPos).Normalize();
        }

        float targetFov = Lerp(fovMin_, fovMax_, speedRatio);

        auto *inputHandler = player_->GetComponent3D<PlayerInputHandler>();
        const bool isGravitySwitching = inputHandler && inputHandler->IsGravitySwitching();
        if (isGravitySwitching) {
            cameraPos = playerPos - forward * gravitySwitchFollowDistance_;
            targetFov = 0.7f;

            if (const auto &requested = inputHandler->GetRequestedGravityDirection();
                requested.has_value() && requested->LengthSquared() > 0.000001f) {
                const Vector3 gravityAim = requested->Normalize();
                lookDir = (forward + gravityAim).Normalize();
            } else {
                // 重力変更入力が未確定の間は、重力方向へは向けずプレイヤー注視を維持
                lookDir = (playerPos + up * lookAtHeight - cameraPos).Normalize();
            }
        }

        const float fallSpeed = std::max(0.0f, playerMovement->GetGravityVelocity().Dot(gravity));
        const float fallTiltRatio = std::clamp(fallSpeed / std::max(0.0001f, fallSpeedForMaxTilt_), 0.0f, 1.0f);
        if (!isGravitySwitching && fallTiltRatio > 0.0f) {
            const Vector3 lookTarget = playerPos + up * lookAtHeight + gravity * (maxLookDownOffset_ * fallTiltRatio);
            lookDir = (lookTarget - cameraPos).Normalize();
        }

        if (inputHandler && inputHandler->IsRearConfirming()) {
            lookDir = -forward;
        }

        float landingImpact = 0.0f;
        if (playerMovement->ConsumeLandingImpact(landingImpact) && landingImpact > landingImpactThreshold_) {
            const float t = std::clamp((landingImpact - landingImpactThreshold_) / std::max(0.0001f, landingImpactForMaxShake_ - landingImpactThreshold_), 0.0f, 1.0f);
            const float shakeScale = std::pow(t, 0.6f);
            cameraController->Shake(maxShakeAmplitude_ * shakeScale, maxShakeDuration_ * shakeScale);

            if (auto *ctx = GetOwnerContext()) {
                if (auto *groundGenerator = ctx->GetComponent<StageGroundGenerator>()) {
                    const float radius = groundReactionBaseRadius_ + landingImpact * groundReactionRadiusPerImpact_;
                    groundGenerator->TriggerGroundReaction(playerPos, radius);
                }
            }
        }

        const Vector3 lateralVelocity = playerMovement->GetLateralVelocity();
        const float lateralSpeed = lateralVelocity.Dot(right);
        const float lateralMax = std::max(0.0001f, playerMovement->GetLateralMaxSpeed());
        const float lateralRatio = std::clamp(lateralSpeed / lateralMax, -1.0f, 1.0f);

        if (!isGravitySwitching && !(inputHandler && inputHandler->IsRearConfirming())) {
            const float lateralLookAmount = lateralLookMaxOffset_ * lateralRatio;
            if (std::abs(lateralLookAmount) > 0.000001f) {
                lookDir = (lookDir + right * lateralLookAmount).Normalize();
            }
        }

        const float lateralTiltAngle = lateralTiltMaxAngleRad_ * lateralRatio;
        Vector3 tiltedUp = up;
        if (std::abs(lateralTiltAngle) > 0.000001f && lookDir.LengthSquared() > 0.000001f) {
            const Quaternion qTilt = Quaternion().MakeRotateAxisAngle(lookDir.Normalize(), lateralTiltAngle);
            tiltedUp = qTilt.RotateVector(up).Normalize();
        }

        cameraController->SetTargetTranslate(cameraPos);
        cameraController->SetTargetRotateQuaternion(ComputeQuaternionFromForwardUp(lookDir, tiltedUp));
        cameraController->SetTargetFovY(targetFov);
        cameraController->SetLerpFactorMove(cameraLerpMove_);
        cameraController->SetLerpFactorRotate(cameraLerpRotate_);
        cameraController->SetLerpFactorFov(cameraLerpFov_);
    }

#if defined(USE_IMGUI)
    void ShowImGui() override {
        if (!ImGui::CollapsingHeader("CameraToPlayerSync")) return;

        ImGui::InputFloat("Follow Distance Min", &followDistanceMin_);
        ImGui::InputFloat("Follow Distance Max", &followDistanceMax_);
        ImGui::InputFloat("Follow Height Min", &followHeightMin_);
        ImGui::InputFloat("Follow Height Max", &followHeightMax_);
        ImGui::InputFloat("LookAt Height Min", &lookAtHeightMin_);
        ImGui::InputFloat("LookAt Height Max", &lookAtHeightMax_);
        ImGui::InputFloat("Fov Min", &fovMin_);
        ImGui::InputFloat("Fov Max", &fovMax_);
        ImGui::InputFloat("Gravity Switch Follow Distance", &gravitySwitchFollowDistance_);
        ImGui::InputFloat("Fall Speed For Max Tilt", &fallSpeedForMaxTilt_);
        ImGui::InputFloat("Max Look Down Offset", &maxLookDownOffset_);

        ImGui::Separator();
        ImGui::InputFloat("Landing Impact Threshold", &landingImpactThreshold_);
        ImGui::InputFloat("Landing Impact For Max Shake", &landingImpactForMaxShake_);
        ImGui::InputFloat("Max Shake Amplitude", &maxShakeAmplitude_);
        ImGui::InputFloat("Max Shake Duration", &maxShakeDuration_);
        ImGui::InputFloat("Ground Reaction Base Radius", &groundReactionBaseRadius_);
        ImGui::InputFloat("Ground Reaction Radius Per Impact", &groundReactionRadiusPerImpact_);

        ImGui::Separator();
        ImGui::Checkbox("Clear View Enabled", &clearViewEnabled_);
        ImGui::InputFloat("Clear View Right Distance", &clearViewRightDistance_);
        ImGui::InputFloat("Clear View Forward Distance", &clearViewForwardDistance_);
        ImGui::InputFloat("Clear View Height", &clearViewHeight_);
        ImGui::InputFloat("Clear View Look Offset Right", &clearViewLookOffsetRight_);

        ImGui::Separator();
        ImGui::InputFloat("Lateral Look Max Offset", &lateralLookMaxOffset_);
        ImGui::InputFloat("Lateral Tilt Max Angle Rad", &lateralTiltMaxAngleRad_);

        ImGui::Separator();
        ImGui::InputFloat("Camera Lerp Move", &cameraLerpMove_);
        ImGui::InputFloat("Camera Lerp Rotate", &cameraLerpRotate_);
        ImGui::InputFloat("Camera Lerp Fov", &cameraLerpFov_);

        ImGui::Separator();
        char paramJsonPathBuffer[512]{};
        std::snprintf(paramJsonPathBuffer, sizeof(paramJsonPathBuffer), "%s", paramJsonFilePath_.c_str());
        if (ImGui::InputText("Param Json Path", paramJsonPathBuffer, sizeof(paramJsonPathBuffer))) {
            paramJsonFilePath_ = paramJsonPathBuffer;
        }
        ImGui::Checkbox("Load Params On Initialize", &loadParamsOnInitialize_);

        if (ImGui::Button("Load CameraToPlayerSync Params")) {
            lastParamIoSucceeded_ = LoadParametersFromJson(paramJsonFilePath_);
        }
        ImGui::SameLine();
        if (ImGui::Button("Save CameraToPlayerSync Params")) {
            lastParamIoSucceeded_ = SaveParametersToJson(paramJsonFilePath_);
        }
        ImGui::Text("Last Param I/O: %s", lastParamIoSucceeded_ ? "Success" : "Failed");
    }
#endif

private:
    static constexpr float kPi = 3.14159265358979323846f;

    static Quaternion MakeFromToQuaternion(const Vector3 &from, const Vector3 &to) {
        const Vector3 f = from.Normalize();
        const Vector3 t = to.Normalize();
        const float dot = std::clamp(f.Dot(t), -1.0f, 1.0f);

        if (dot > 0.9999f) {
            return Quaternion::Identity();
        }

        if (dot < -0.9999f) {
            Vector3 axis = f.Cross(Vector3{1.0f, 0.0f, 0.0f});
            if (axis.LengthSquared() <= 0.000001f) {
                axis = f.Cross(Vector3{0.0f, 1.0f, 0.0f});
            }
            axis = axis.Normalize();
            return Quaternion().MakeRotateAxisAngle(axis, kPi);
        }

        Vector3 axis = f.Cross(t);
        Quaternion q(axis.x, axis.y, axis.z, 1.0f + dot);
        return q.Normalize();
    }

    static Quaternion ComputeQuaternionFromForwardUp(const Vector3 &desiredForward, const Vector3 &desiredUp) {
        const Vector3 defaultForward{0.0f, 0.0f, 1.0f};
        const Vector3 defaultUp{0.0f, 1.0f, 0.0f};

        Quaternion qForward = MakeFromToQuaternion(defaultForward, desiredForward);

        Vector3 currentUp = qForward.RotateVector(defaultUp);
        Vector3 upProjected = desiredUp - desiredForward * desiredUp.Dot(desiredForward);
        Vector3 currentUpProjected = currentUp - desiredForward * currentUp.Dot(desiredForward);

        if (upProjected.LengthSquared() > 0.000001f && currentUpProjected.LengthSquared() > 0.000001f) {
            upProjected = upProjected.Normalize();
            currentUpProjected = currentUpProjected.Normalize();

            const Vector3 cross = currentUpProjected.Cross(upProjected);
            const float dot = std::clamp(currentUpProjected.Dot(upProjected), -1.0f, 1.0f);
            const float angle = std::atan2(cross.Dot(desiredForward), dot);
            const Quaternion qRoll = Quaternion().MakeRotateAxisAngle(desiredForward, angle);
            qForward = (qRoll * qForward).Normalize();
        }

        return qForward;
    }

    JSON BuildParamsJson() const {
        JSON j;
        j["followDistanceMin"] = followDistanceMin_;
        j["followDistanceMax"] = followDistanceMax_;
        j["followHeightMin"] = followHeightMin_;
        j["followHeightMax"] = followHeightMax_;
        j["lookAtHeightMin"] = lookAtHeightMin_;
        j["lookAtHeightMax"] = lookAtHeightMax_;
        j["fovMin"] = fovMin_;
        j["fovMax"] = fovMax_;
        j["gravitySwitchFollowDistance"] = gravitySwitchFollowDistance_;
        j["fallSpeedForMaxTilt"] = fallSpeedForMaxTilt_;
        j["maxLookDownOffset"] = maxLookDownOffset_;
        j["landingImpactThreshold"] = landingImpactThreshold_;
        j["landingImpactForMaxShake"] = landingImpactForMaxShake_;
        j["maxShakeAmplitude"] = maxShakeAmplitude_;
        j["maxShakeDuration"] = maxShakeDuration_;
        j["groundReactionBaseRadius"] = groundReactionBaseRadius_;
        j["groundReactionRadiusPerImpact"] = groundReactionRadiusPerImpact_;
        j["clearViewEnabled"] = clearViewEnabled_;
        j["clearViewRightDistance"] = clearViewRightDistance_;
        j["clearViewForwardDistance"] = clearViewForwardDistance_;
        j["clearViewHeight"] = clearViewHeight_;
        j["clearViewLookOffsetRight"] = clearViewLookOffsetRight_;
        j["lateralLookMaxOffset"] = lateralLookMaxOffset_;
        j["lateralTiltMaxAngleRad"] = lateralTiltMaxAngleRad_;
        j["cameraLerpMove"] = cameraLerpMove_;
        j["cameraLerpRotate"] = cameraLerpRotate_;
        j["cameraLerpFov"] = cameraLerpFov_;
        return j;
    }

    void ApplyParamsJson(const JSON &j) {
        followDistanceMin_ = j.value("followDistanceMin", followDistanceMin_);
        followDistanceMax_ = j.value("followDistanceMax", followDistanceMax_);
        followHeightMin_ = j.value("followHeightMin", followHeightMin_);
        followHeightMax_ = j.value("followHeightMax", followHeightMax_);
        lookAtHeightMin_ = j.value("lookAtHeightMin", lookAtHeightMin_);
        lookAtHeightMax_ = j.value("lookAtHeightMax", lookAtHeightMax_);
        fovMin_ = j.value("fovMin", fovMin_);
        fovMax_ = j.value("fovMax", fovMax_);
        gravitySwitchFollowDistance_ = j.value("gravitySwitchFollowDistance", gravitySwitchFollowDistance_);
        fallSpeedForMaxTilt_ = j.value("fallSpeedForMaxTilt", fallSpeedForMaxTilt_);
        maxLookDownOffset_ = j.value("maxLookDownOffset", maxLookDownOffset_);
        landingImpactThreshold_ = j.value("landingImpactThreshold", landingImpactThreshold_);
        landingImpactForMaxShake_ = j.value("landingImpactForMaxShake", landingImpactForMaxShake_);
        maxShakeAmplitude_ = j.value("maxShakeAmplitude", maxShakeAmplitude_);
        maxShakeDuration_ = j.value("maxShakeDuration", maxShakeDuration_);
        groundReactionBaseRadius_ = j.value("groundReactionBaseRadius", groundReactionBaseRadius_);
        groundReactionRadiusPerImpact_ = j.value("groundReactionRadiusPerImpact", groundReactionRadiusPerImpact_);
        clearViewEnabled_ = j.value("clearViewEnabled", clearViewEnabled_);
        clearViewRightDistance_ = j.value("clearViewRightDistance", clearViewRightDistance_);
        clearViewForwardDistance_ = j.value("clearViewForwardDistance", clearViewForwardDistance_);
        clearViewHeight_ = j.value("clearViewHeight", clearViewHeight_);
        clearViewLookOffsetRight_ = j.value("clearViewLookOffsetRight", clearViewLookOffsetRight_);
        lateralLookMaxOffset_ = j.value("lateralLookMaxOffset", lateralLookMaxOffset_);
        lateralTiltMaxAngleRad_ = j.value("lateralTiltMaxAngleRad", lateralTiltMaxAngleRad_);
        cameraLerpMove_ = j.value("cameraLerpMove", cameraLerpMove_);
        cameraLerpRotate_ = j.value("cameraLerpRotate", cameraLerpRotate_);
        cameraLerpFov_ = j.value("cameraLerpFov", cameraLerpFov_);
    }

    bool SaveParametersToJson(const std::string &path) const {
        try {
            const std::filesystem::path p(path);
            if (p.has_parent_path()) {
                std::filesystem::create_directories(p.parent_path());
            }
            return SaveJSON(BuildParamsJson(), path, 4);
        } catch (...) {
            return false;
        }
    }

    bool LoadParametersFromJson(const std::string &path) {
        try {
            const JSON j = LoadJSON(path);
            if (j.is_discarded() || !j.is_object()) {
                return false;
            }
            ApplyParamsJson(j);
            return true;
        } catch (...) {
            return false;
        }
    }

    Object3DBase *player_ = nullptr;

    float followDistanceMin_ = 1.0f;
    float followDistanceMax_ = 2.0f;
    float followHeightMin_ = 2.0f;
    float followHeightMax_ = 6.0f;
    float lookAtHeightMin_ = 2.0f;
    float lookAtHeightMax_ = 6.0f;
    float fovMin_ = 0.8f;
    float fovMax_ = 2.5f;
    float gravitySwitchFollowDistance_ = 10.0f;
    float fallSpeedForMaxTilt_ = 128.0f;
    float maxLookDownOffset_ = 4.0f;

    float landingImpactThreshold_ = 6.0f;
    float landingImpactForMaxShake_ = 64.0f;
    float maxShakeAmplitude_ = 1.4f;
    float maxShakeDuration_ = 0.5f;

    float groundReactionBaseRadius_ = 8.0f;
    float groundReactionRadiusPerImpact_ = 0.6f;

    bool clearViewEnabled_ = false;
    float clearViewRightDistance_ = 2.0f;
    float clearViewForwardDistance_ = 6.0f;
    float clearViewHeight_ = 2.0f;
    float clearViewLookOffsetRight_ = -1.5f;

    float lateralLookMaxOffset_ = 0.25f;
    float lateralTiltMaxAngleRad_ = 0.25f;

    float cameraLerpMove_ = 0.9f;
    float cameraLerpRotate_ = 0.05f;
    float cameraLerpFov_ = 0.1f;

    std::string paramJsonFilePath_ = "Assets/Application/CameraToPlayerSyncParam.json";
    bool loadParamsOnInitialize_ = true;
    bool lastParamIoSucceeded_ = true;
};

} // namespace KashipanEngine
