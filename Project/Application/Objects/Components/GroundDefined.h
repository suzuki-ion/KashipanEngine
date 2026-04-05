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
            mat->SetSampler(SamplerManager::GetSampler(DefaultSampler::LinearWrap));
            mat->SetTexture(TextureManager::GetTextureFromFileName("grid.png"));
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

        auto uv = mat->GetUVTransform();
        const Vector3 scale = tr->GetScale();
        uv.scale = Vector3{scale.x * 0.125f, scale.y * 0.125f, scale.z * 0.125f};
        mat->SetUVTransform(uv);
        return true;
    }

#if defined(USE_IMGUI)
    void ShowImGui() override {}
#endif

private:
    Collider *collider_ = nullptr;
};

} // namespace KashipanEngine
