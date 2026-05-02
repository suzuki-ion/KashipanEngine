#pragma once

#include <KashipanEngine.h>
#include "Objects/Components/PlayerMovementControllerAccess.h"
#include "Objects/Components/PlayerGravityFallBehavior.h"
#include "Objects/Components/PlayerForwardMoveBehavior.h"
#include "Objects/Components/PlayerLateralMoveBehavior.h"
#include "Objects/Components/PlayerJumpBehavior.h"
#include "Objects/Components/PlayerCollisionBehavior.h"
#include "Objects/Components/GroundDefined.h"

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

        groundedThisFrame_ = false;
        hasLandingImpact_ = false;
        lastLandingImpact_ = 0.0f;

        if (movementLocked_) {
            if (forwardBehavior_) {
                forwardBehavior_->ForwardSpeedRef() = 0.0f;
            }
            if (lateralBehavior_) {
                lateralBehavior_->LateralVelocityRef() = Vector3{0.0f, 0.0f, 0.0f};
                lateralBehavior_->ClearLateralInput();
            }
            if (gravityBehavior_) {
                gravityBehavior_->GravityVelocityRef() = Vector3{0.0f, 0.0f, 0.0f};
                gravityBehavior_->SetFastFallEnabled(false);
            }
            if (jumpBehavior_) {
                jumpBehavior_->SetJumpInputHeld(false);
            }
            return true;
        }

        const Vector3 prevFramePosition = tr->GetTranslate();

        // 着地イベント算出用に、重力変更前の落下蓄積量を保持
        const float fallDistanceBeforeGravityChange = accumulatedFallDistance_;

		// プレイヤー操作による重力変更は、着地イベント算出のために落下距離計測をリセットする必要があるため、Updateの最初の方で処理する
        if (collisionBehavior_) {
            if (auto requestedGravity = collisionBehavior_->ConsumeRequestedGravityDirection(); requestedGravity.has_value()) {
                SetGravityDirection(*requestedGravity);
            }
        }

		// 着地判定と落下距離の計測
        const bool grounded = collisionBehavior_ ? collisionBehavior_->ConsumeGrounded() : false;
        groundedThisFrame_ = grounded;

		// 地面にいない場合は落下距離を蓄積
        if (!grounded) {
            const Vector3 down = gravityDirection_.Normalize();
            const float fallSpeed = gravityBehavior_ ? gravityBehavior_->GetGravityVelocity().Dot(down) : 0.0f;
            if (fallSpeed > 0.0f) {
                accumulatedFallDistance_ += fallSpeed * dt;
            }
        }

        // ジャンプ処理は重力変更前の落下距離計測を正しく行うために、重力変更処理の前に行う必要がある
        if (jumpBehavior_ && gravityBehavior_) {
            jumpBehavior_->Apply(dt, gravityDirection_, gravityBehavior_->GravityVelocityRef());
        }

        // 重力処理は落下処理として最初に行う
        if (gravityBehavior_) {
            gravityBehavior_->SetFastFallEnabled(fastFallEnabled_ && !grounded);
            gravityBehavior_->Apply(dt, gravityDirection_);
        }

        Vector3 gravityVelocity{0.0f, 0.0f, 0.0f};
        if (gravityBehavior_) gravityVelocity = gravityBehavior_->GetGravityVelocity();

        const Vector3 gravityFramePosition = tr->GetTranslate() + gravityVelocity * dt;
        tr->SetTranslate(gravityFramePosition);

        // まず重力による移動後の突き抜けを押し戻す（落下処理の押し戻し）
        if (collisionBehavior_ && gravityBehavior_) {
            Vector3 pos = tr->GetTranslate();
            auto &gvRef = gravityBehavior_->GravityVelocityRef();
            collisionBehavior_->ResolveStayTranslationAndVelocity(pos, gvRef);
            tr->SetTranslate(pos);
            gravityVelocity = gvRef;
        }

        // 前方移動と横移動は落下処理と押し戻しの後に行う
        if (forwardBehavior_ && gravityBehavior_) {
            forwardBehavior_->Apply(dt, grounded, gravityBehavior_->GetGravityVelocity(), gravityDirection_);
        }

        // 横移動は前方移動の速度に応じて最大速度が変化するため、前方移動の更新後に行う必要がある
        if (lateralBehavior_ && forwardBehavior_) {
            lateralBehavior_->Apply(dt, gravityDirection_, forwardDirection_, forwardBehavior_->GetForwardSpeed(), forwardBehavior_->GetMinForwardSpeed());
        }

        // 移動処理
        Vector3 lateralVelocity{0.0f, 0.0f, 0.0f};
        float forwardSpeed = 0.0f;
        if (lateralBehavior_) lateralVelocity = lateralBehavior_->GetLateralVelocity();
        if (forwardBehavior_) forwardSpeed = forwardBehavior_->GetForwardSpeed();

        // 前方移動、横移動の合成速度で位置を更新
        Vector3 totalVelocity = forwardDirection_ * forwardSpeed + lateralVelocity;
        if (grounded && collisionBehavior_) {
            const Vector3 groundNormal = collisionBehavior_->GetGroundNormal().Normalize();
            const float intoGround = totalVelocity.Dot(groundNormal);
            if (intoGround < 0.0f) {
                totalVelocity -= groundNormal * intoGround;
            }
        }
        const Vector3 currentFramePosition = tr->GetTranslate() + totalVelocity * dt;
        tr->SetTranslate(currentFramePosition);

        // 前方/横移動による衝突の補正。ここで lastCollisionTime を使ってヒット時刻に沿って補正。
        if (collisionBehavior_ && gravityBehavior_) {
            Vector3 correctedPos = tr->GetTranslate();
            if (auto hitTime = collisionBehavior_->ConsumeLastCollisionTime(); hitTime.has_value()) {
                const float t = std::clamp(*hitTime, 0.0f, 1.0f);
                correctedPos = Lerp(currentFramePosition, prevFramePosition, t);
                correctedPos.z = currentFramePosition.z;
            }
            auto &gvRef = gravityBehavior_->GravityVelocityRef();
            collisionBehavior_->ResolveStayTranslationAndVelocity(correctedPos, gvRef);
            tr->SetTranslate(correctedPos);
            gravityVelocity = gvRef;
        }

		// 着地イベントの算出
        if (grounded && !wasGroundedPrev_) {
            const float landingImpact = std::max(accumulatedFallDistance_, fallDistanceBeforeGravityChange);

			// ジャンプカウントのリセットは着地イベント算出後に行う必要がある
            if (jumpBehavior_) {
                jumpBehavior_->ResetJumpCount();
            }

			// 着地イベントの算出のために、着地前の落下距離と重力変更前の落下距離の大きい方を着地衝撃とする
            const bool canRecoverGauge = collisionBehavior_ ? collisionBehavior_->ConsumeLastGroundWasFirstTouch() : false;

			// 着地衝撃に応じて重力ゲージを回復。着地イベントが発生したフレームでのみ回復可能とするため、着地イベントの算出後に行う
            if (canRecoverGauge) {
                const float recovered = std::clamp(
                    landingGaugeRecoveryBase_ + landingImpact * landingGaugeRecoveryPerDistance_,
                    0.0f,
                    gravityGaugeMax_);
                gravityGauge_ = std::clamp(gravityGauge_ + recovered, 0.0f, gravityGaugeMax_);
            }

			// 着地イベントの算出のために、着地衝撃を保存
            hasLandingImpact_ = true;
            lastLandingImpact_ = landingImpact;
            accumulatedFallDistance_ = 0.0f;

            // 着地した地面がSlowGroundだった場合は減速する
            if (collisionBehavior_ && collisionBehavior_->IsOnSlowGround()) {
                if (forwardBehavior_) {
                    forwardBehavior_->ForwardSpeedRef() *= slowGroundSpeedMultiplier_;
                }
                if (lateralBehavior_) {
                    lateralBehavior_->LateralVelocityRef() *= slowGroundSpeedMultiplier_;
                }
			}

		} else if (!grounded && wasGroundedPrev_) {// 離地したフレームで落下距離計測をリセット
            accumulatedFallDistance_ = 0.0f;
            if (collisionBehavior_) {
                (void)collisionBehavior_->ConsumeLastGroundWasFirstTouch();
            }
        }
        wasGroundedPrev_ = grounded;

        gravityChangeBlend_ = std::max(0.0f, gravityChangeBlend_ - dt / std::max(0.0001f, gravityChangeBlendDuration_));

        UpdateRotation();
        return true;
    }

    void MoveRight(float value = 1.0f) {
        if (movementLocked_) return;
        if (lateralBehavior_) {
            lateralBehavior_->MoveRight(value);
        }
    }

    void MoveLeft(float value = 1.0f) {
        if (movementLocked_) return;
        if (lateralBehavior_) {
            lateralBehavior_->MoveLeft(value);
        }
    }

    void Jump() {
        if (movementLocked_) return;
        if (jumpBehavior_) {
            jumpBehavior_->RequestJump();
            // ジャンプ開始時は落下距離計測をリセット
            accumulatedFallDistance_ = 0.0f;
        }
    }

    void SetJumpInputHeld(bool held) {
        if (jumpBehavior_) {
            jumpBehavior_->SetJumpInputHeld(held);
        }
    }

    bool TryUseGravityGaugeAndSetGravityDirection(const Vector3 &direction) {
        if (movementLocked_) return false;
        if (!CanUseGravityChange()) return false;
        const Vector3 dir = direction.Normalize();
        if (dir.LengthSquared() <= 0.000001f) return false;
        if (dir == gravityDirection_) return false;

        gravityGauge_ = std::max(0.0f, gravityGauge_ - gravityGaugePerUse_);
        // プレイヤー操作による重力変更時は落下距離計測をリセット
        accumulatedFallDistance_ = 0.0f;
        SetGravityDirection(direction);
        gravityChangeBlend_ = 1.0f;
        return true;
    }

    bool CanUseGravityChange() const {
        if (movementLocked_) return false;
        return gravityGauge_ >= gravityGaugePerUse_;
    }

    void SetMovementLocked(bool locked) {
        movementLocked_ = locked;
        if (movementLocked_) {
            if (forwardBehavior_) {
                forwardBehavior_->ForwardSpeedRef() = 0.0f;
            }
            if (lateralBehavior_) {
                lateralBehavior_->LateralVelocityRef() = Vector3{0.0f, 0.0f, 0.0f};
                lateralBehavior_->ClearLateralInput();
            }
            if (gravityBehavior_) {
                gravityBehavior_->GravityVelocityRef() = Vector3{0.0f, 0.0f, 0.0f};
                gravityBehavior_->SetFastFallEnabled(false);
            }
            if (jumpBehavior_) {
                jumpBehavior_->SetJumpInputHeld(false);
            }
        }
    }
    bool IsMovementLocked() const { return movementLocked_; }

    void SetCollisionResponseEnabled(bool enabled) {
        if (collisionBehavior_) {
            collisionBehavior_->SetCollisionResponseEnabled(enabled);
        }
    }
    bool IsCollisionResponseEnabled() const {
        return collisionBehavior_ ? collisionBehavior_->IsCollisionResponseEnabled() : true;
    }

    const Vector3 &GetGravityDirection() const { return gravityDirection_; }
    const Vector3 &GetForwardDirection() const { return forwardDirection_; }
    float GetForwardSpeed() const { return forwardBehavior_ ? forwardBehavior_->GetForwardSpeed() : 0.0f; }
    Vector3 GetLateralVelocity() const { return lateralBehavior_ ? lateralBehavior_->GetLateralVelocity() : Vector3{0.0f, 0.0f, 0.0f}; }
    Vector3 GetGravityVelocity() const { return gravityBehavior_ ? gravityBehavior_->GetGravityVelocity() : Vector3{0.0f, 0.0f, 0.0f}; }
    void SetForwardSpeed(float v) {
        if (forwardBehavior_) {
            forwardBehavior_->ForwardSpeedRef() = std::max(0.0f, v);
        }
    }
    void SetLateralVelocity(const Vector3 &v) {
        if (lateralBehavior_) {
            lateralBehavior_->LateralVelocityRef() = v;
        }
    }
    void SetGravityVelocity(const Vector3 &v) {
        if (gravityBehavior_) {
            gravityBehavior_->GravityVelocityRef() = v;
        }
    }
    void SetFastFallEnabled(bool enabled) {
        fastFallEnabled_ = enabled;
        if (!enabled && gravityBehavior_) {
            gravityBehavior_->SetFastFallEnabled(false);
        }
    }
    float GetGravityGauge() const { return gravityGauge_; }
    float GetGravityGaugePerUse() const { return gravityGaugePerUse_; }
    float GetGravityGaugeMax() const { return gravityGaugeMax_; }
    float GetGravityGaugeNormalized() const {
        if (gravityGaugeMax_ <= 0.000001f) return 0.0f;
        return std::clamp(gravityGauge_ / gravityGaugeMax_, 0.0f, 1.0f);
    }
    void SetGravityGauge(float value) {
        gravityGauge_ = std::clamp(value, 0.0f, gravityGaugeMax_);
    }
    float GetGravityChangeBlend() const { return gravityChangeBlend_; }
    float GetAccumulatedFallDistance() const { return accumulatedFallDistance_; }
    bool ConsumeLandingImpact(float &outImpact) {
        if (!hasLandingImpact_) return false;
        outImpact = lastLandingImpact_;
        return true;
    }

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
        if (jumpBehavior_) {
            jumpBehavior_->ResetJumpCount();
        }
        accumulatedFallDistance_ = 0.0f;
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
    int GetJumpCount() const { return jumpBehavior_ ? jumpBehavior_->GetJumpCount() : 0; }
    int GetMaxJumpCount() const { return jumpBehavior_ ? jumpBehavior_->GetMaxJumpCount() : 0; }
    float GetMaxJumpInputHoldTime() const { return jumpBehavior_ ? jumpBehavior_->GetMaxJumpInputHoldTime() : 0.0f; }

    void MarkGrounded() override { /* collision behavior manages grounded state */ }
    bool ConsumeGrounded() override { return groundedThisFrame_; }

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

    bool wasGroundedPrev_ = false;
    bool groundedThisFrame_ = false;
    float accumulatedFallDistance_ = 0.0f;
    bool hasLandingImpact_ = false;
    float lastLandingImpact_ = 0.0f;

    float gravityGaugePerUse_ = 16.0f;
    int gravityGaugeUseCountPerFull_ = 2;
    float gravityGaugeMax_ = gravityGaugePerUse_ * static_cast<float>(gravityGaugeUseCountPerFull_);
    float gravityGauge_ = gravityGaugeMax_;
    float landingGaugeRecoveryBase_ = 1.0f;
    float landingGaugeRecoveryPerDistance_ = 0.05f;
	float slowGroundSpeedMultiplier_ = 0.7f;

    float gravityChangeBlend_ = 0.0f;
    float gravityChangeBlendDuration_ = 0.35f;
    bool movementLocked_ = false;
    bool fastFallEnabled_ = false;
};

} // namespace KashipanEngine
