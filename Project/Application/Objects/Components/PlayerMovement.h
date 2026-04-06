#pragma once

#include <KashipanEngine.h>
#include "Objects/Components/CollisionAttributes.h"

#include <algorithm>
#include <cmath>

namespace KashipanEngine {

class PlayerMovement final : public IObjectComponent3D {
public:
    explicit PlayerMovement(Collider *collider)
        : IObjectComponent3D("PlayerMovement", 1), collider_(collider) {}

    ~PlayerMovement() override = default;

    std::unique_ptr<IObjectComponent> Clone() const override {
        auto ptr = std::make_unique<PlayerMovement>(collider_);
        ptr->gravityDirection_ = gravityDirection_;
        ptr->forwardDirection_ = forwardDirection_;
        ptr->gravityVelocity_ = gravityVelocity_;
        ptr->lateralVelocity_ = lateralVelocity_;
        ptr->forwardSpeed_ = forwardSpeed_;
        return ptr;
    }

    std::optional<bool> Initialize() override {
        auto *ctx = GetOwner3DContext();
        if (!ctx) return false;

        if (collider_ && !ctx->GetComponent<Collision3D>()) {
            ColliderInfo3D info{};
            Math::OBB obb{};
            obb.center = Vector3{0.0f, 0.0f, 0.0f};
            obb.halfSize = Vector3{0.5f, 0.5f, 0.5f};
            obb.orientation = Matrix4x4::Identity();
            info.shape = obb;
            info.attribute.set(CollisionAttribute::Player);
            info.ignoreAttribute.set(CollisionAttribute::Player);
            info.onCollisionEnter = [this](const HitInfo3D &hit) { OnCollisionEnter(hit); };
            info.onCollisionStay = [this](const HitInfo3D &hit) { OnCollisionStay(hit); };
            if (!ctx->RegisterComponent<Collision3D>(collider_, info)) {
                return false;
            }
        }

        if (auto *mat = ctx->GetComponent<Material3D>()) {
            mat->SetEnableLighting(false);
        }

        UpdateRotation();
        return true;
    }

    std::optional<bool> Update() override {
        auto *ctx = GetOwner3DContext();
        if (!ctx) return false;

        auto *tr = ctx->GetComponent<Transform3D>();
        if (!tr) return false;

        const float dt = std::max(0.0f, GetDeltaTime() * GetGameSpeed());
        const Vector3 down = gravityDirection_.Normalize();
        const bool isFalling = gravityVelocity_.Dot(down) > 0.0f;

        isGrounded_ = false;

        if (isFalling) {
            forwardSpeed_ = std::min(maxForwardSpeed_, forwardSpeed_ + forwardAcceleration_ * dt);
        } else {
            forwardSpeed_ = std::max(minForwardSpeed_, forwardSpeed_ - groundDeceleration_ * dt);
        }
        const Vector3 up = -down;

        Vector3 right = down.Cross(forwardDirection_);
        if (right.LengthSquared() <= 0.000001f) {
            right = Vector3{1.0f, 0.0f, 0.0f};
        } else {
            right = right.Normalize();
        }

        const float boostedLateralMaxSpeed = lateralMaxSpeed_
            + std::max(0.0f, forwardSpeed_ - minForwardSpeed_) * lateralSpeedPerForward_;
        const Vector3 desiredLateral = right * (std::clamp(lateralInput_, -1.0f, 1.0f) * boostedLateralMaxSpeed);
        const float lateralLerp = std::clamp(lateralAcceleration_ * dt, 0.0f, 1.0f);
        lateralVelocity_ = Vector3::Lerp(lateralVelocity_, desiredLateral, lateralLerp);

        gravityVelocity_ += down * (gravityPower_ * dt);

        const Vector3 totalVelocity = forwardDirection_ * forwardSpeed_ + lateralVelocity_ + gravityVelocity_;
        tr->SetTranslate(tr->GetTranslate() + totalVelocity * dt);

        lateralInput_ = 0.0f;

        UpdateRotation();
        return true;
    }

    void MoveRight(float value = 1.0f) {
        lateralInput_ = std::min(lateralInput_, -value);
    }

    void MoveLeft(float value = 1.0f) {
        lateralInput_ = std::max(lateralInput_, value);
    }

    void Jump() {
        gravityVelocity_ = (-gravityDirection_.Normalize()) * jumpPower_;
    }

    void SetGravityDirection(const Vector3 &direction) {
        if (direction.LengthSquared() <= 0.000001f) return;
        const Vector3 newDirection = direction.Normalize();
        if (newDirection == gravityDirection_) return;

        gravityDirection_ = newDirection;
        gravityVelocity_ = Vector3{0.0f, 0.0f, 0.0f};
        UpdateRotation();
    }

    const Vector3 &GetGravityDirection() const { return gravityDirection_; }
    const Vector3 &GetForwardDirection() const { return forwardDirection_; }
    float GetForwardSpeed() const { return forwardSpeed_; }
    float GetMinForwardSpeed() const { return minForwardSpeed_; }
    float GetMaxForwardSpeed() const { return maxForwardSpeed_; }

#if defined(USE_IMGUI)
    void ShowImGui() override {}
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

    bool IsGroundObject(Object3DBase *obj) const {
        if (!obj) return false;
        return obj->GetComponent3D("GroundDefined") != nullptr;
    }

    void OnCollisionEnter(const HitInfo3D &hit) {
        if (!IsGroundObject(hit.otherObject)) return;

        const Vector3 normal = hit.normal.Normalize();
        if (normal.LengthSquared() <= 0.000001f) return;

        isGrounded_ = true;

        const Vector3 forward = forwardDirection_.Normalize();
        if ((normal - (-forward)).LengthSquared() <= 0.0001f ||
            (normal - forward).LengthSquared() <= 0.0001f) {
            return;
        }

        // 触れた面の法線をそのまま重力方向へ反映する
        SetGravityDirection(-normal);
    }

    void OnCollisionStay(const HitInfo3D &hit) {
        if (!IsGroundObject(hit.otherObject)) return;

        const Vector3 normal = hit.normal.Normalize();
        if (normal.LengthSquared() <= 0.000001f) return;

        isGrounded_ = true;

        auto *ctx = GetOwner3DContext();
        if (!ctx) return;
        auto *tr = ctx->GetComponent<Transform3D>();
        if (!tr) return;

        tr->SetTranslate(tr->GetTranslate() + normal * hit.penetration);

        const float vn = gravityVelocity_.Dot(normal);
        if (vn < 0.0f) {
            gravityVelocity_ -= normal * vn;
        }
    }

    void UpdateRotation() {
        auto *ctx = GetOwner3DContext();
        if (!ctx) return;
        auto *tr = ctx->GetComponent<Transform3D>();
        if (!tr) return;

        const Vector3 down = gravityDirection_.Normalize();
        const Vector3 defaultDown{0.0f, -1.0f, 0.0f};
        const Vector3 defaultForward{0.0f, 0.0f, 1.0f};

        const Quaternion qDown = MakeFromToQuaternion(defaultDown, down);
        Vector3 currentForward = qDown.RotateVector(defaultForward);

        const Vector3 up = -down;
        Vector3 targetForward = forwardDirection_ - up * forwardDirection_.Dot(up);
        if (targetForward.LengthSquared() <= 0.000001f) {
            targetForward = currentForward;
        }
        targetForward = targetForward.Normalize();

        currentForward = currentForward - up * currentForward.Dot(up);
        if (currentForward.LengthSquared() <= 0.000001f) {
            currentForward = targetForward;
        }
        currentForward = currentForward.Normalize();

        const Vector3 cross = currentForward.Cross(targetForward);
        const float dot = std::clamp(currentForward.Dot(targetForward), -1.0f, 1.0f);
        const float signedAngle = std::atan2(cross.Dot(up), dot);

        const Quaternion qTwist = Quaternion().MakeRotateAxisAngle(up, signedAngle);
        const Quaternion finalQ = (qTwist * qDown).Normalize();

        tr->SetRotateQuaternion(finalQ);
    }

    Collider *collider_ = nullptr;

    Vector3 gravityDirection_{0.0f, -1.0f, 0.0f};
    Vector3 forwardDirection_{0.0f, 0.0f, -1.0f};

    Vector3 gravityVelocity_{0.0f, 0.0f, 0.0f};
    Vector3 lateralVelocity_{0.0f, 0.0f, 0.0f};

    float lateralInput_ = 0.0f;

    float gravityPower_ = 128.0f;

    float forwardSpeed_ = 32.0f;
    float minForwardSpeed_ = 16.0f;
    float maxForwardSpeed_ = 64.0f;
    float forwardAcceleration_ = 8.0f;
    float groundDeceleration_ = 4.0f;

    float lateralMaxSpeed_ = 16.0f;
    float lateralAcceleration_ = 8.0f;
    float lateralSpeedPerForward_ = 0.25f;

    float jumpPower_ = 64.0f;

    bool isGrounded_ = false;
};

} // namespace KashipanEngine
