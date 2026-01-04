#pragma once

#include "Objects/IObjectComponent.h"
#include "Objects/Collision/Collider.h"

#include <memory>
#include <optional>

namespace KashipanEngine {

class Collision3D final : public IObjectComponent3D {
public:
    static const std::string &GetStaticComponentType() {
        static const std::string type = "Collision3D";
        return type;
    }

    Collision3D(Collider *collider, const ColliderInfo3D &info = {})
        : IObjectComponent3D("Collision3D", 1), collider_(collider), info_(info) {}

    ~Collision3D() override = default;

    std::unique_ptr<IObjectComponent> Clone() const override {
        auto ptr = std::make_unique<Collision3D>(collider_, info_);
        return ptr;
    }

    std::optional<bool> Initialize() override {
        if (!collider_) return false;
        if (colliderId_ != 0) return true;
        colliderId_ = collider_->Add(info_);
        return true;
    }
    std::optional<bool> Finalize() override {
        if (collider_ && colliderId_ != 0) {
            collider_->Remove3D(colliderId_);
        }
        colliderId_ = 0;
        return true;
    }

    const ColliderInfo3D &GetInfo() const { return info_; }
    ColliderInfo3D &GetInfo() { return info_; }

    Collider *GetCollider() const { return collider_; }
    Collider::ColliderID GetColliderID() const { return colliderId_; }

    void SetCollider(Collider *collider) { collider_ = collider; }

#if defined(USE_IMGUI)
    void ShowImGui() override {}
#endif

private:
    Collider *collider_ = nullptr;
    ColliderInfo3D info_{};
    Collider::ColliderID colliderId_ = 0;
};

} // namespace KashipanEngine
