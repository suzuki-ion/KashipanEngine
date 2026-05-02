#pragma once

#include <KashipanEngine.h>
#include "Objects/Components/CollisionAttributes.h"

#include <algorithm>

namespace KashipanEngine {

class CoinDefined final : public IObjectComponent3D {
public:
    explicit CoinDefined(Collider *collider)
        : IObjectComponent3D("CoinDefined", 1), collider_(collider) {}

    ~CoinDefined() override = default;

    std::unique_ptr<IObjectComponent> Clone() const override {
        return std::make_unique<CoinDefined>(collider_);
    }

    std::optional<bool> Initialize() override {
        auto *ctx = GetOwner3DContext();
        if (!ctx) return false;

        if (auto *mat = ctx->GetComponent<Material3D>()) {
            mat->SetColor(defaultColor_);
        }

        if (auto *tr = ctx->GetComponent<Transform3D>()) {
            initialTranslate_ = tr->GetTranslate();
            initialRotate_ = tr->GetRotate();
            initialScale_ = tr->GetScale();
            initialParent_ = tr->GetParentTransform();
        }

        if (collider_ && !ctx->GetComponent<Collision3D>()) {
            ColliderInfo3D info{};
            Math::Sphere sphere{};
            sphere.center = Vector3{0.0f, 0.0f, 0.0f};
            sphere.radius = collisionRadius_;
            info.shape = sphere;
            info.attribute.set(CollisionAttribute::Coin);
            info.ignoreAttribute.set(CollisionAttribute::Ground);
            info.ignoreAttribute.set(CollisionAttribute::Coin);
            info.onCollisionEnter = [this](const HitInfo3D &hit) { OnCollisionEnter(hit); };
            if (!ctx->RegisterComponent<Collision3D>(collider_, info)) {
                return false;
            }
        }

        ResetCollectAnimation();
        return true;
    }

    std::optional<bool> Update() override {
        auto *ctx = GetOwner3DContext();
        if (!ctx) return false;

        auto *tr = ctx->GetComponent<Transform3D>();
        auto *mat = ctx->GetComponent<Material3D>();
        if (!tr || !mat) return true;

        const float dt = std::max(0.0f, GetDeltaTime() * GetGameSpeed());

        if (!hasCollected_) {
            Vector3 rot = tr->GetRotate();
            rot.y += idleRotationSpeed_ * dt;
            tr->SetRotate(rot);
            return true;
        }

        if (!isAnimating_) {
            return true;
        }

        elapsed_ = std::min(elapsed_ + dt, totalDuration_);

        const float goDuration = raiseDuration_;
        const float backDuration = fallDuration_;
        const float raiseEnd = goDuration;

        const float raiseT = std::clamp(elapsed_ / goDuration, 0.0f, 1.0f);
        const float fallT = std::clamp((elapsed_ - raiseEnd) / backDuration, 0.0f, 1.0f);

        float y = 0.0f;
        float rotateY = 0.0f;
        Vector3 scale = Vector3{1.0f, 1.0f, 1.0f};
        Vector4 color = touchColorStart_;

        if (elapsed_ <= raiseEnd) {
            const float easedT = Apply(raiseT, EaseType::EaseOutCubic);
            y = Lerp(startTranslate_.y, raiseTranslate_.y, easedT);
            rotateY = Lerp(startRotateY_, startRotateY_ + rotationDelta_, easedT);
            color = Lerp(touchColorStart_, touchColorEnd_, raiseT);
        } else {
            const float easedT = Apply(fallT, EaseType::EaseInCubic);
            y = Lerp(raiseTranslate_.y, endTranslate_.y, easedT);
            scale = Lerp(startScale_, endScale_, easedT);
            color = touchColorEnd_;
        }

        Vector3 newTranslate = tr->GetTranslate();
        newTranslate.y = y;
        tr->SetTranslate(newTranslate);

        Vector3 rot = tr->GetRotate();
        rot.y = rotateY;
        tr->SetRotate(rot);
        tr->SetScale(scale);
        mat->SetColor(color);

        if (elapsed_ >= totalDuration_) {
            isAnimating_ = false;
            tr->SetTranslate(endTranslate_);
            tr->SetScale(endScale_);
        }

        return true;
    }

    void ResetCollectAnimation() {
        auto *ctx = GetOwner3DContext();
        if (!ctx) return;

        auto *tr = ctx->GetComponent<Transform3D>();
        auto *mat = ctx->GetComponent<Material3D>();
        if (!tr || !mat) return;

        hasCollected_ = false;
        isAnimating_ = false;
        elapsed_ = 0.0f;
        playerTransform_ = nullptr;

        tr->SetParentTransform(initialParent_);
        tr->SetTranslate(initialTranslate_);
        tr->SetScale(initialScale_);
        tr->SetRotate(initialRotate_);
        mat->SetColor(defaultColor_);
    }

    bool HasCollected() const { return hasCollected_; }

#if defined(USE_IMGUI)
    void ShowImGui() override {}
#endif

private:
    void OnCollisionEnter(const HitInfo3D &hit) {
        if (hasCollected_) return;
        if (!hit.otherObject) return;
        if (!hit.otherObject->GetComponent3D("PlayerMovementController")) return;

        auto *ctx = GetOwner3DContext();
        if (!ctx) return;

        auto *tr = ctx->GetComponent<Transform3D>();
        auto *mat = ctx->GetComponent<Material3D>();
        if (!tr || !mat) return;

        playerTransform_ = hit.otherObject->GetComponent3D<Transform3D>();
        if (!playerTransform_) return;

        hasCollected_ = true;
        isAnimating_ = true;
        elapsed_ = 0.0f;

        tr->SetParentTransform(playerTransform_);
        tr->SetTranslate(startTranslate_);
        tr->SetScale(startScale_);

        Vector3 rot = tr->GetRotate();
        rot.y = 0.0f;
        tr->SetRotate(rot);
        startRotateY_ = 0.0f;
        mat->SetColor(touchColorStart_);
    }

    Collider *collider_ = nullptr;
    Transform3D *playerTransform_ = nullptr;

    const float collisionRadius_ = 2.0f;
    const float idleRotationSpeed_ = 4.0f;

    const float raiseDuration_ = 0.5f;
    const float fallDuration_ = 0.5f;
    const float totalDuration_ = 1.0f;

    const Vector3 startTranslate_{0.0f, 2.0f, 0.0f};
    const Vector3 raiseTranslate_{0.0f, 3.0f, 0.0f};
    const Vector3 endTranslate_{0.0f, 2.0f, 0.0f};

    const Vector3 startScale_{1.0f, 1.0f, 1.0f};
    const Vector3 endScale_{0.0f, 0.0f, 0.0f};

    const Vector4 defaultColor_{1.0f, 1.0f, 1.0f, 1.0f};
    const Vector4 touchColorStart_{1.0f, 1.0f, 1.0f, 1.0f};
    const Vector4 touchColorEnd_{0.5f, 1.0f, 0.5f, 1.0f};

    Vector3 initialTranslate_{0.0f, 0.0f, 0.0f};
    Vector3 initialRotate_{0.0f, 0.0f, 0.0f};
    Vector3 initialScale_{1.0f, 1.0f, 1.0f};
    Transform3D *initialParent_ = nullptr;

    float elapsed_ = 0.0f;
    float startRotateY_ = 0.0f;
    const float rotationDelta_ = 6.28318530718f;

    bool hasCollected_ = false;
    bool isAnimating_ = false;
};

} // namespace KashipanEngine
