#pragma once
#include "Objects/ObjectComponentHeader.h"
#include "Objects/ObjectContext.h"
#include "Objects/Components/3D/Transform3D.h"
#include "Utilities/MathUtils.h"
#include <algorithm>
#include <cmath>
#include <functional>
#include <optional>

namespace KashipanEngine {

/// @brief LookAt制約コンポーネント
class LookAtConstraint final : public IObjectComponent3D {
public:
    LookAtConstraint() : IObjectComponent3D("LookAtConstraint", 1) {}
    ~LookAtConstraint() override = default;

    std::unique_ptr<IObjectComponent> Clone() const override {
        auto ptr = std::make_unique<LookAtConstraint>();
        return ptr;
    }

    std::optional<bool> Update() override {
        static const Vector3 forward(0.0f, 0.0f, 1.0f);

        auto *transform = GetOwner3DContext()->GetComponent<Transform3D>();
        if (!transform) return false;

        const Vector3 selfPos = transform->GetTranslate();
        const Vector3 targetPos = GetTarget();
        const Vector3 toTarget = targetPos - selfPos;
        if (toTarget.LengthSquared() == 0.0f) return true;

        const Vector3 dir = toTarget.Normalize();
        const float dot = std::clamp(forward.Dot(dir), -1.0f, 1.0f);

        Quaternion lookQuat = Quaternion::Identity();
        if (dot > 0.9999f) {
            lookQuat = Quaternion::Identity();
        } else if (dot < -0.9999f) {
            lookQuat = Quaternion::MakeRotateAxisAngle(Vector3(0.0f, 1.0f, 0.0f), static_cast<float>(M_PI));
        } else {
            const Vector3 axis = forward.Cross(dir).Normalize();
            const float angle = std::acos(dot);
            lookQuat = Quaternion::MakeRotateAxisAngle(axis, angle);
        }

        const Quaternion offsetQuat = Quaternion::MakeRotateEuler(offsetRotate_);
        Quaternion finalQuat = (lookQuat * offsetQuat).Normalize();

        Vector3 finalEuler = QuaternionToEuler(finalQuat);
        const Vector3 currentEuler = transform->GetRotate();
        if (!isRotateX_) finalEuler.x = currentEuler.x;
        if (!isRotateY_) finalEuler.y = currentEuler.y;
        if (!isRotateZ_) finalEuler.z = currentEuler.z;

        finalQuat = Quaternion::MakeRotateEuler(finalEuler);
        transform->SetRotateQuaternion(finalQuat);

        return true;
    }

    void SetTarget(const Vector3 &target) { target_ = target; }
    void SetTargetFunc(const std::function<const Vector3 &()> &func) { targetFunc_ = func; }
    void SetOffsetRotate(const Vector3 &offset) { offsetRotate_ = offset; }

    const Vector3 &GetTarget() const {
        if (targetFunc_) {
            return targetFunc_();
        } else if (target_) {
            return *target_;
        } else {
            static const Vector3 defaultTarget(0.0f, 0.0f, 0.0f);
            return defaultTarget;
        }
    }
    const Vector3 &GetOffsetRotate() const { return offsetRotate_; }

#ifdef USE_IMGUI
    void ShowImGui() override {
        ImGui::TextUnformatted(Translation("engine.imgui.component.lookatconstraint").c_str());
        Vector3 offset = GetOffsetRotate() * 180.0f / 3.14159265f;
        if (ImGui::DragFloat3(Translation("engine.imgui.lookatconstraint.offsetrotate").c_str(), &offset.x, 0.01f, -180.0f, 180.0f)) {
            SetOffsetRotate(offset * 3.14159265f / 180.0f);
        }
        ImGui::Checkbox(Translation("engine.imgui.lookatconstraint.rotatex").c_str(), &isRotateX_);
        ImGui::Checkbox(Translation("engine.imgui.lookatconstraint.rotatey").c_str(), &isRotateY_);
        ImGui::Checkbox(Translation("engine.imgui.lookatconstraint.rotatez").c_str(), &isRotateZ_);
    }
#endif

private:
    static Vector3 QuaternionToEuler(const Quaternion &q) {
        Matrix4x4 mat = q.MakeRotateMatrix();
        float rotX, rotY, rotZ;

        float sy = mat.m[0][2];
        if (sy > 0.9999f) {
            rotY = 3.14159265f * 0.5f;
            rotX = std::atan2(mat.m[1][0], mat.m[1][1]);
            rotZ = 0.0f;
        } else if (sy < -0.9999f) {
            rotY = -3.14159265f * 0.5f;
            rotX = std::atan2(-mat.m[1][0], mat.m[1][1]);
            rotZ = 0.0f;
        } else {
            rotY = std::asin(sy);
            rotX = std::atan2(-mat.m[1][2], mat.m[2][2]);
            rotZ = std::atan2(-mat.m[0][1], mat.m[0][0]);
        }
        return Vector3(rotX, rotY, rotZ);
    }

    std::optional<Vector3> target_;
    std::function<const Vector3 &()> targetFunc_;
    Vector3 offsetRotate_;

    bool isRotateX_ = true;
    bool isRotateY_ = true;
    bool isRotateZ_ = true;
};

REGISTER_COMPONENT_OBJECT3D(LookAtConstraint)

} // namespace KashipanEngine