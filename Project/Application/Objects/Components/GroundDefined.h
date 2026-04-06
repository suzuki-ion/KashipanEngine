#pragma once

#include <KashipanEngine.h>
#include "Objects/Components/CollisionAttributes.h"

namespace KashipanEngine {

class GroundDefined final : public IObjectComponent3D {
public:
    explicit GroundDefined(Collider *collider)
        : IObjectComponent3D("GroundDefined", 1), collider_(collider) {}

    ~GroundDefined() override = default;

    std::unique_ptr<IObjectComponent> Clone() const override {
        return std::make_unique<GroundDefined>(collider_);
    }

    std::optional<bool> Initialize() override {
        auto *ctx = GetOwner3DContext();
        if (!ctx) return false;

        if (auto *mat = ctx->GetComponent<Material3D>()) {
            mat->SetEnableLighting(false);
            mat->SetColor(defaultColor_);
            mat->SetSampler(SamplerManager::GetSampler(DefaultSampler::LinearWrap));
            mat->SetTexture(TextureManager::GetTextureFromFileName("square_alpha.png"));
        }

        if (collider_ && !ctx->GetComponent<Collision3D>()) {
            ColliderInfo3D info{};
            Math::OBB obb{};
            obb.center = Vector3{0.0f, 0.0f, 0.0f};
            obb.halfSize = Vector3{0.5f, 0.5f, 0.5f};
            obb.orientation = Matrix4x4::Identity();
            info.shape = obb;
            info.attribute.set(CollisionAttribute::Ground);
            info.ignoreAttribute.set(CollisionAttribute::Ground);
            info.onCollisionEnter = [this](const HitInfo3D &hit) { OnCollisionEnter(hit); };
            if (!ctx->RegisterComponent<Collision3D>(collider_, info)) {
                return false;
            }
        }

        return true;
    }

    std::optional<bool> Update() override {
        auto *ctx = GetOwner3DContext();
        if (!ctx) return false;

        auto *tr = ctx->GetComponent<Transform3D>();
        auto *mat = ctx->GetComponent<Material3D>();
        if (!tr || !mat) return true;

        if (isTouchColorAnimating_) {
            const float dt = std::max(0.0f, GetDeltaTime() * GetGameSpeed());
            touchColorAnimT_ = Normalize01(touchColorAnimT_ + dt, 0.0f, touchColorAnimDuration_);
            const Vector4 color = Lerp(touchColorStart_, touchColorEnd_, touchColorAnimT_);
            mat->SetColor(color);

            if (touchColorAnimT_ >= 1.0f) {
                isTouchColorAnimating_ = false;
                mat->SetColor(touchColorEnd_);
            }
        }

        return true;
    }

#if defined(USE_IMGUI)
    void ShowImGui() override {}
#endif

    void ResetTouchColorAnimation() {
        hasPlayedTouchColorAnimation_ = false;
        isTouchColorAnimating_ = false;
        touchColorAnimT_ = 0.0f;
        if (auto *ctx = GetOwner3DContext()) {
            if (auto *mat = ctx->GetComponent<Material3D>()) {
                mat->SetColor(defaultColor_);
            }
        }
    }

private:
    void OnCollisionEnter(const HitInfo3D &hit) {
        if (hasPlayedTouchColorAnimation_) return;
        if (!hit.otherObject) return;
        if (!hit.otherObject->GetComponent3D("PlayerMovementController")) return;

        hasPlayedTouchColorAnimation_ = true;
        isTouchColorAnimating_ = true;
        touchColorAnimT_ = 0.0f;

        if (auto *ctx = GetOwner3DContext()) {
            if (auto *mat = ctx->GetComponent<Material3D>()) {
                mat->SetColor(touchColorStart_);
            }
        }
    }

    Collider *collider_ = nullptr;

    const Vector4 defaultColor_{0.0f, 0.5f, 0.0f, 1.0f};
    const Vector4 touchColorStart_{1.0f, 1.0f, 1.0f, 1.0f};
    const Vector4 touchColorEnd_{0.5f, 1.0f, 0.5f, 1.0f};

    bool hasPlayedTouchColorAnimation_ = false;
    bool isTouchColorAnimating_ = false;
    float touchColorAnimT_ = 0.0f;
    float touchColorAnimDuration_ = 1.0f;
};

} // namespace KashipanEngine
