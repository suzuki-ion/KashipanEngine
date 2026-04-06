#pragma once

#include <KashipanEngine.h>
#include "Objects/Components/PlayerMovementControllerAccess.h"
#include "Objects/Components/PlayerGravityFallBehavior.h"
#include "Objects/Components/PlayerForwardMoveBehavior.h"
#include "Objects/Components/PlayerLateralMoveBehavior.h"
#include "Objects/Components/PlayerJumpBehavior.h"
#include "Objects/Components/PlayerCollisionBehavior.h"

#include <algorithm>
#include <cmath>

namespace KashipanEngine {

class PlayerMovementController final : public IObjectComponent3D, public IPlayerMovementControllerAccess {
public:
    explicit PlayerMovementController(Collider *collider)
        : IObjectComponent3D("PlayerMovementController", 1), collider_(collider) {}

    ~PlayerMovementController() override = default;

    std::unique_ptr<IObjectComponent> Clone() const override {
        auto ptr = std::make_unique<PlayerMovementController>(collider_);
        ptr->gravityDirection_ = gravityDirection_;
        ptr->forwardDirection_ = forwardDirection_;
        return ptr;
    }

    std::optional<bool> Initialize() override {
        auto *ctx = GetOwner3DContext();
        if (!ctx) return false;

        if (!ctx->GetComponent<PlayerCollisionBehavior>()) {
            (void)ctx->RegisterComponent<PlayerCollisionBehavior>(collider_);
        }
        if (!ctx->GetComponent<PlayerGravityFallBehavior>()) {
            (void)ctx->RegisterComponent<PlayerGravityFallBehavior>();
        }
        if (!ctx->GetComponent<PlayerForwardMoveBehavior>()) {
            (void)ctx->RegisterComponent<PlayerForwardMoveBehavior>();
        }
        if (!ctx->GetComponent<PlayerLateralMoveBehavior>()) {
            (void)ctx->RegisterComponent<PlayerLateralMoveBehavior>();
        }
        if (!ctx->GetComponent<PlayerJumpBehavior>()) {
            (void)ctx->RegisterComponent<PlayerJumpBehavior>();
        }

        collisionBehavior_ = ctx->GetComponent<PlayerCollisionBehavior>();
        gravityBehavior_ = ctx->GetComponent<PlayerGravityFallBehavior>();
        forwardBehavior_ = ctx->GetComponent<PlayerForwardMoveBehavior>();
        lateralBehavior_ = ctx->GetComponent<PlayerLateralMoveBehavior>();
        jumpBehavior_ = ctx->GetComponent<PlayerJumpBehavior>();

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

        const float dt = std::clamp(GetDeltaTime() * GetGameSpeed(), 0.0f, 0.1f);

        if (collisionBehavior_) {
            if (auto requestedGravity = collisionBehavior_->ConsumeRequestedGravityDirection(); requestedGravity.has_value()) {
                SetGravityDirection(*requestedGravity);
            }
        }

        const bool grounded = collisionBehavior_ ? collisionBehavior_->ConsumeGrounded() : false;

        if (jumpBehavior_ && gravityBehavior_) {
            jumpBehavior_->Apply(gravityDirection_, gravityBehavior_->GravityVelocityRef());
        }

        if (forwardBehavior_ && gravityBehavior_) {
            forwardBehavior_->Apply(dt, grounded, gravityBehavior_->GetGravityVelocity(), gravityDirection_);
        }
        if (lateralBehavior_ && forwardBehavior_) {
            lateralBehavior_->Apply(dt, gravityDirection_, forwardDirection_, forwardBehavior_->GetForwardSpeed(), forwardBehavior_->GetMinForwardSpeed());
        }
        if (gravityBehavior_) {
            gravityBehavior_->Apply(dt, gravityDirection_);
        }

        Vector3 gravityVelocity{0.0f, 0.0f, 0.0f};
        Vector3 lateralVelocity{0.0f, 0.0f, 0.0f};
        float forwardSpeed = 0.0f;
        if (gravityBehavior_) gravityVelocity = gravityBehavior_->GetGravityVelocity();
        if (lateralBehavior_) lateralVelocity = lateralBehavior_->GetLateralVelocity();
        if (forwardBehavior_) forwardSpeed = forwardBehavior_->GetForwardSpeed();

        if (collisionBehavior_ && gravityBehavior_) {
            Vector3 correctedPos = tr->GetTranslate();
            auto &gvRef = gravityBehavior_->GravityVelocityRef();
            collisionBehavior_->ResolveStayTranslationAndVelocity(correctedPos, gvRef);
            tr->SetTranslate(correctedPos);
            gravityVelocity = gvRef;
        }

        const Vector3 totalVelocity = forwardDirection_ * forwardSpeed + lateralVelocity + gravityVelocity;
        tr->SetTranslate(tr->GetTranslate() + totalVelocity * dt);
        UpdateRotation();
        return true;
    }

    void MoveRight(float value = 1.0f) {
        if (lateralBehavior_) {
            lateralBehavior_->MoveRight(value);
        }
    }

    void MoveLeft(float value = 1.0f) {
        if (lateralBehavior_) {
            lateralBehavior_->MoveLeft(value);
        }
    }

    void Jump() {
        if (jumpBehavior_) {
            jumpBehavior_->RequestJump();
        }
    }

    const Vector3 &GetGravityDirection() const { return gravityDirection_; }
    const Vector3 &GetForwardDirection() const { return forwardDirection_; }
    float GetForwardSpeed() const { return forwardBehavior_ ? forwardBehavior_->GetForwardSpeed() : 0.0f; }

#if defined(USE_IMGUI)
    void ShowImGui() override {}
#endif

    // IPlayerMovementControllerAccess
    Vector3 GetGravityDirectionValue() const override { return gravityDirection_; }
    Vector3 GetForwardDirectionValue() const override { return forwardDirection_; }

    void SetGravityDirection(const Vector3 &direction) override {
        if (direction.LengthSquared() <= 0.000001f) return;
        const Vector3 newDirection = direction.Normalize();
        if (newDirection == gravityDirection_) return;

        gravityDirection_ = newDirection;
        if (gravityBehavior_) {
            gravityBehavior_->SetGravityVelocity(Vector3{0.0f, 0.0f, 0.0f});
        }
        UpdateRotation();
    }

    Vector3 &GravityVelocityRef() override {
        static Vector3 dummy{0.0f, 0.0f, 0.0f};
        return gravityBehavior_ ? gravityBehavior_->GravityVelocityRef() : dummy;
    }
    Vector3 &LateralVelocityRef() override {
        static Vector3 dummy{0.0f, 0.0f, 0.0f};
        return lateralBehavior_ ? lateralBehavior_->LateralVelocityRef() : dummy;
    }
    float &ForwardSpeedRef() override {
        static float dummy = 0.0f;
        return forwardBehavior_ ? forwardBehavior_->ForwardSpeedRef() : dummy;
    }

    float GetMinForwardSpeed() const override { return forwardBehavior_ ? forwardBehavior_->GetMinForwardSpeed() : 0.0f; }
    float GetMaxForwardSpeed() const override { return forwardBehavior_ ? forwardBehavior_->GetMaxForwardSpeed() : 0.0f; }
    float GetForwardAcceleration() const override { return forwardBehavior_ ? forwardBehavior_->GetForwardAcceleration() : 0.0f; }
    float GetForwardAccelPerFallSpeed() const override { return forwardBehavior_ ? forwardBehavior_->GetForwardAccelPerFallSpeed() : 0.0f; }
    float GetGroundDeceleration() const override { return forwardBehavior_ ? forwardBehavior_->GetGroundDeceleration() : 0.0f; }

    float GetLateralMaxSpeed() const override { return lateralBehavior_ ? lateralBehavior_->GetLateralMaxSpeed() : 0.0f; }
    float GetLateralAcceleration() const override { return lateralBehavior_ ? lateralBehavior_->GetLateralAcceleration() : 0.0f; }
    float GetLateralSpeedPerForward() const override { return lateralBehavior_ ? lateralBehavior_->GetLateralSpeedPerForward() : 0.0f; }
    float GetLateralInput() const override { return lateralBehavior_ ? lateralBehavior_->GetLateralInput() : 0.0f; }
    void ClearLateralInput() override { if (lateralBehavior_) lateralBehavior_->ClearLateralInput(); }

    float GetJumpPower() const override { return jumpBehavior_ ? jumpBehavior_->GetJumpPower() : 0.0f; }

    void MarkGrounded() override { /* collision behavior manages grounded state */ }
    bool ConsumeGrounded() override { return collisionBehavior_ ? collisionBehavior_->ConsumeGrounded() : false; }

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
        tr->SetRotateQuaternion((qTwist * qDown).Normalize());
    }

    Collider *collider_ = nullptr;

    Vector3 gravityDirection_{0.0f, -1.0f, 0.0f};
    Vector3 forwardDirection_{0.0f, 0.0f, -1.0f};

    PlayerCollisionBehavior *collisionBehavior_ = nullptr;
    PlayerGravityFallBehavior *gravityBehavior_ = nullptr;
    PlayerForwardMoveBehavior *forwardBehavior_ = nullptr;
    PlayerLateralMoveBehavior *lateralBehavior_ = nullptr;
    PlayerJumpBehavior *jumpBehavior_ = nullptr;
};

} // namespace KashipanEngine
