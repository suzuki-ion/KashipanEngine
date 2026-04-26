#pragma once

#include <KashipanEngine.h>
#include "Objects/Components/CollisionAttributes.h"
#include "Objects/Components/GroundDefined.h"
#include "Objects/Components/SlowGroundDefined.h"
#include "Objects/Components/PlayerMovementControllerAccess.h"
#include <algorithm>

namespace KashipanEngine {

class PlayerCollisionBehavior final : public IObjectComponent3D {
public:
    explicit PlayerCollisionBehavior(Collider *collider)
        : IObjectComponent3D("PlayerCollisionBehavior", 1), collider_(collider) {}

    ~PlayerCollisionBehavior() override = default;

    std::unique_ptr<IObjectComponent> Clone() const override {
        return std::make_unique<PlayerCollisionBehavior>(collider_);
    }

    std::optional<bool> Initialize() override {
        auto *ctx = GetOwner3DContext();
        if (!ctx) return false;

        if (collider_ && !ctx->GetComponent<Collision3D>()) {
            ColliderInfo3D info{};
            Math::OBB obb{};
            obb.center = Vector3{0.0f, 0.0f, 0.0f};
            obb.halfSize = Vector3{0.5f, 1.0f, 0.5f};
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

        return true;
    }

    bool ConsumeGrounded() {
        const bool v = grounded_;
        grounded_ = false;
        return v;
    }

    std::optional<Vector3> ConsumeRequestedGravityDirection() {
        auto out = requestedGravityDirection_;
        requestedGravityDirection_.reset();
        return out;
    }

    const Vector3 &GetGroundNormal() const { return lastGroundNormal_; }
    Object3DBase *GetLastGroundObject() const { return lastGroundObject_; }
    bool ConsumeLastGroundWasFirstTouch() {
        const bool v = lastGroundWasFirstTouch_;
        lastGroundWasFirstTouch_ = false;
        return v;
    }

    void ResolveStayTranslationAndVelocity(Vector3 &position, Vector3 &gravityVelocity) {
        if (!hasStayCorrection_) return;

        position += stayCorrection_;
        const float vn = gravityVelocity.Dot(lastGroundNormal_);
        if (vn < 0.0f) {
            gravityVelocity -= lastGroundNormal_ * vn;
        }

        hasStayCorrection_ = false;
        stayCorrection_ = Vector3{0.0f, 0.0f, 0.0f};
    }

#if defined(USE_IMGUI)
    void ShowImGui() override {}
#endif

	bool IsOnSlowGround() const { return isSlowGround_; }

private:
    static bool IsGroundObject(Object3DBase *obj) {
        if (!obj) return false;
        return obj->GetComponent3D("GroundDefined") != nullptr;
    }

    static bool IsFirstTouchGroundAtCollision(Object3DBase *obj) {
        if (!obj) return false;
        auto *ground = obj->GetComponent3D<GroundDefined>();
        if (!ground) return false;

        // コールバック順の差異を吸収:
        // - Ground側が先なら playerTouchEvent が true
        // - Player側が先なら まだ未タッチ状態
        return ground->HasPlayerTouchEvent() || !ground->HasBeenTouchedByPlayer();
    }

    void OnCollisionEnter(const HitInfo3D &hit) {
        if (!IsGroundObject(hit.otherObject)) return;

        const Vector3 normal = hit.normal.Normalize();
        if (normal.LengthSquared() <= 0.000001f) return;

        grounded_ = true;
        lastGroundNormal_ = normal;
        lastGroundObject_ = hit.otherObject;
        lastGroundWasFirstTouch_ = lastGroundWasFirstTouch_ || IsFirstTouchGroundAtCollision(hit.otherObject);

        auto *ctx = GetOwner3DContext();
        if (!ctx) return;
        auto *base = ctx->GetComponent("PlayerMovementController");
        auto *controller = dynamic_cast<IPlayerMovementControllerAccess *>(base);
        if (!controller) return;

        const Vector3 forward = controller->GetForwardDirectionValue().Normalize();
        if ((normal - (-forward)).LengthSquared() <= 0.0001f ||
            (normal - forward).LengthSquared() <= 0.0001f) {
            return;
        }

        requestedGravityDirection_ = -normal;

        if(auto *slowGround = hit.otherObject->GetComponent3D<SlowGroundDefined>()) {
            isSlowGround_ = true;
        } else {
            isSlowGround_ = false;
		}
    }

    void OnCollisionStay(const HitInfo3D &hit) {
        if (!IsGroundObject(hit.otherObject)) return;

        const Vector3 normal = hit.normal.Normalize();
        if (normal.LengthSquared() <= 0.000001f) return;

        grounded_ = true;
        lastGroundNormal_ = normal;
        lastGroundObject_ = hit.otherObject;
        lastGroundWasFirstTouch_ = lastGroundWasFirstTouch_ || IsFirstTouchGroundAtCollision(hit.otherObject);

        const float penetration = std::clamp(hit.penetration, 0.0f, maxStayPenetrationPerHit_);
        if (penetration <= 0.0f) return;

        const float currentAlongNormal = stayCorrection_.Dot(normal);
        if (penetration > currentAlongNormal) {
            stayCorrection_ += normal * (penetration - currentAlongNormal);
        }

        const float correctionLengthSq = stayCorrection_.LengthSquared();
        const float maxCorrectionSq = maxStayCorrectionPerFrame_ * maxStayCorrectionPerFrame_;
        if (correctionLengthSq > maxCorrectionSq) {
            stayCorrection_ = stayCorrection_.Normalize() * maxStayCorrectionPerFrame_;
        }

        hasStayCorrection_ = stayCorrection_.LengthSquared() > 0.000001f;
    }

    Collider *collider_ = nullptr;
    bool grounded_ = false;
    std::optional<Vector3> requestedGravityDirection_{};
    Vector3 lastGroundNormal_{0.0f, 1.0f, 0.0f};
    Object3DBase *lastGroundObject_ = nullptr;
    bool lastGroundWasFirstTouch_ = false;
    Vector3 stayCorrection_{0.0f, 0.0f, 0.0f};
    bool hasStayCorrection_ = false;
	bool isSlowGround_ = false;
    float maxStayPenetrationPerHit_ = 0.2f;
    float maxStayCorrectionPerFrame_ = 0.3f;
};

} // namespace KashipanEngine
