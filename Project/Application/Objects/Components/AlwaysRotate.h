#pragma once

#include <KashipanEngine.h>

namespace KashipanEngine {

class AlwaysRotate final : public IObjectComponent3D {
public:
    explicit AlwaysRotate(const Vector3 &angularVelocity = Vector3{0.0f, 0.0f, 1.0f})
        : IObjectComponent3D("AlwaysRotate", 1), angularVelocity_(angularVelocity) {}

    ~AlwaysRotate() override = default;

    std::unique_ptr<IObjectComponent> Clone() const override {
        return std::make_unique<AlwaysRotate>(angularVelocity_);
    }

    std::optional<bool> Update() override {
        auto *ctx = GetOwner3DContext();
        if (!ctx) return false;

        auto *tr = ctx->GetComponent<Transform3D>();
        if (!tr) return false;

        const float dt = std::max(0.0f, GetDeltaTime() * GetGameSpeed());
        tr->SetRotate(tr->GetRotate() + angularVelocity_ * dt);
        return true;
    }

    void SetAngularVelocity(const Vector3 &v) { angularVelocity_ = v; }
    const Vector3 &GetAngularVelocity() const { return angularVelocity_; }

#if defined(USE_IMGUI)
    void ShowImGui() override {}
#endif

private:
    Vector3 angularVelocity_{0.0f, 0.0f, 1.0f};
};

} // namespace KashipanEngine
