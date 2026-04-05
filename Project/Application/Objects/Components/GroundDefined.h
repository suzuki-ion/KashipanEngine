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
            mat->SetColor(Vector4{ 0.0f, 1.0f, 0.0f, 1.0f });
            mat->SetSampler(SamplerManager::GetSampler(DefaultSampler::LinearWrap));
            mat->SetTexture(TextureManager::GetTextureFromFileName("square_alpha.png"));
        }

        if (collider_ && !ctx->GetComponent<Collision3D>()) {
            ColliderInfo3D info{};
            info.shape = Math::AABB{Vector3{-0.5f, -0.5f, -0.5f}, Vector3{0.5f, 0.5f, 0.5f}};
            info.attribute.set(CollisionAttribute::Ground);
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

       /* auto uv = mat->GetUVTransform();
        const Vector3 scale = tr->GetScale();
        float scaleX = scale.x > scale.y ? scale.x * 0.05f : scale.z * 0.05f;
        float scaleY = scale.y > scale.x ? scale.y * 0.05f : scale.z * 0.05f;
        uv.scale = Vector3{ scaleX, scaleY, 1.0f };
        mat->SetUVTransform(uv);*/
        return true;
    }

#if defined(USE_IMGUI)
    void ShowImGui() override {}
#endif

private:
    Collider *collider_ = nullptr;
};

} // namespace KashipanEngine
