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
            info.ccdEnabled = true;
            if (!ctx->RegisterComponent<Collision3D>(collider_, info)) {
                return false;
            }
        }

        return true;
    }

    bool ConsumeGrounded() {
        if (!collisionResponseEnabled_) {
            grounded_ = false;
            return false;
        }
        const bool v = grounded_;
        grounded_ = false;
        return v;
    }

    std::optional<Vector3> ConsumeRequestedGravityDirection() {
        if (!collisionResponseEnabled_) {
            requestedGravityDirection_.reset();
            return std::nullopt;
        }
        auto out = requestedGravityDirection_;
        requestedGravityDirection_.reset();
        return out;
    }

    const Vector3 &GetGroundNormal() const { return lastGroundNormal_; }
    Object3DBase *GetLastGroundObject() const { return lastGroundObject_; }
    bool ConsumeLastGroundWasFirstTouch() {
        if (!collisionResponseEnabled_) {
            lastGroundWasFirstTouch_ = false;
            return false;
        }
        const bool v = lastGroundWasFirstTouch_;
        lastGroundWasFirstTouch_ = false;
        return v;
    }

    std::optional<float> ConsumeLastCollisionTime() {
        if (!collisionResponseEnabled_) {
            lastCollisionTime_.reset();
            return std::nullopt;
        }
        auto out = lastCollisionTime_;
        lastCollisionTime_.reset();
        return out;
    }

    void SetCollisionResponseEnabled(bool enabled) {
        collisionResponseEnabled_ = enabled;
        if (!collisionResponseEnabled_) {
            ResetTransientCollisionState();
        }
    }
    bool IsCollisionResponseEnabled() const { return collisionResponseEnabled_; }

    void ResolveStayTranslationAndVelocity(Vector3 &position, Vector3 &gravityVelocity) {
        if (!collisionResponseEnabled_) {
            hasStayCorrection_ = false;
            stayCorrection_ = Vector3{0.0f, 0.0f, 0.0f};
            needsVelocityCorrection_ = false;
            return;
        }

        if (needsVelocityCorrection_) {
            const float vn = gravityVelocity.Dot(lastGroundNormal_);
            if (vn < 0.0f) {
                gravityVelocity -= lastGroundNormal_ * vn;
            }
            needsVelocityCorrection_ = false;
        }

        if (hasStayCorrection_) {
            // Apply at most maxStayCorrectionPerFrame_ this frame to avoid overshooting when penetration is large
            Vector3 correctionToApply = stayCorrection_;
            const float len = correctionToApply.Length();
            if (len > maxStayCorrectionPerFrame_) {
                correctionToApply = correctionToApply.Normalize() * maxStayCorrectionPerFrame_;
            }

            position += correctionToApply;

            // Remove applied portion from accumulated stayCorrection_
            stayCorrection_ -= correctionToApply;
            // If the remaining correction is very small, clear it
            hasStayCorrection_ = stayCorrection_.LengthSquared() > 0.000001f;
            if (!hasStayCorrection_) {
                stayCorrection_ = Vector3{0.0f, 0.0f, 0.0f};
            }
        }
    }

#if defined(USE_IMGUI)
    void ShowImGui() override {}
#endif

	bool IsOnSlowGround() const { return isSlowGround_; }

private:
    void ResetTransientCollisionState() {
        grounded_ = false;
        requestedGravityDirection_.reset();
        lastGroundWasFirstTouch_ = false;
        lastCollisionTime_.reset();
        stayCorrection_ = Vector3{0.0f, 0.0f, 0.0f};
        hasStayCorrection_ = false;
        needsVelocityCorrection_ = false;
        isSlowGround_ = false;
    }

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
        if (!collisionResponseEnabled_) return;
        if (!IsGroundObject(hit.otherObject)) return;

        const Vector3 normal = hit.normal.Normalize();
        if (normal.LengthSquared() <= 0.000001f) return;

        grounded_ = true;
        needsVelocityCorrection_ = true;
        lastGroundNormal_ = normal;
        lastGroundObject_ = hit.otherObject;
        lastGroundWasFirstTouch_ = lastGroundWasFirstTouch_ || IsFirstTouchGroundAtCollision(hit.otherObject);

        if (hit.time >= 0.0f) {
            if (!lastCollisionTime_.has_value() || hit.time < *lastCollisionTime_) {
                lastCollisionTime_ = hit.time;
            }
        }

        const float penetration = hit.penetration;
        if (penetration > 0.0f) {
            const float currentAlongNormal = stayCorrection_.Dot(normal);
            // Limit how much penetration we add per hit to avoid huge instantaneous corrections
            const float addPen = std::min(penetration - currentAlongNormal, maxStayPenetrationPerHit_);
            if (addPen > 0.0f) {
                stayCorrection_ += normal * addPen;
                hasStayCorrection_ = stayCorrection_.LengthSquared() > 0.000001f;
            }
        }

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
        if (!collisionResponseEnabled_) return;
        if (!IsGroundObject(hit.otherObject)) return;

        const Vector3 normal = hit.normal.Normalize();
        if (normal.LengthSquared() <= 0.000001f) return;

        grounded_ = true;
        needsVelocityCorrection_ = true;
        lastGroundNormal_ = normal;
        lastGroundObject_ = hit.otherObject;
        lastGroundWasFirstTouch_ = lastGroundWasFirstTouch_ || IsFirstTouchGroundAtCollision(hit.otherObject);

        if (hit.time >= 0.0f) {
            if (!lastCollisionTime_.has_value() || hit.time < *lastCollisionTime_) {
                lastCollisionTime_ = hit.time;
            }
        }

        const float penetration = hit.penetration;
        if (penetration <= 0.0f) return;

        const float currentAlongNormal = stayCorrection_.Dot(normal);
        // Limit addition per hit to avoid huge spike when velocity is large
        const float addPen = std::min(penetration - currentAlongNormal, maxStayPenetrationPerHit_);
        if (addPen > 0.0f) {
            stayCorrection_ += normal * addPen;
        }

        hasStayCorrection_ = stayCorrection_.LengthSquared() > 0.000001f;
    }

    Collider *collider_ = nullptr;
    bool grounded_ = false;
    std::optional<Vector3> requestedGravityDirection_{};
    Vector3 lastGroundNormal_{0.0f, 1.0f, 0.0f};
    Object3DBase *lastGroundObject_ = nullptr;
    bool lastGroundWasFirstTouch_ = false;
    std::optional<float> lastCollisionTime_{};
	Vector3 stayCorrection_{0.0f, 0.0f, 0.0f};
	bool hasStayCorrection_ = false;
	bool needsVelocityCorrection_ = false;
	bool isSlowGround_ = false;
    bool collisionResponseEnabled_ = true;
    float maxStayPenetrationPerHit_ = 0.2f;
    float maxStayCorrectionPerFrame_ = 0.3f;
};

} // namespace KashipanEngine
