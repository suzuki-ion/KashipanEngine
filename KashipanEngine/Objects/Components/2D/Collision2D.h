#pragma once

#include "Objects/IObjectComponent.h"
#include "Objects/Collision/Collider.h"

#include <memory>
#include <optional>

namespace KashipanEngine {

class Collision2D final : public IObjectComponent2D {
public:
    static const std::string &GetStaticComponentType() {
        static const std::string type = "Collision2D";
        return type;
    }

    Collision2D(Collider *collider, const ColliderInfo2D &info = {})
        : IObjectComponent2D("Collision2D", 1), collider_(collider), info_(info) {}

    ~Collision2D() override = default;

    std::unique_ptr<IObjectComponent> Clone() const override {
        auto ptr = std::make_unique<Collision2D>(collider_, info_);
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
            collider_->Remove2D(colliderId_);
        }
        colliderId_ = 0;
        return true;
    }

    const ColliderInfo2D &GetInfo() const { return info_; }
    ColliderInfo2D &GetInfo() { return info_; }

    Collider *GetCollider() const { return collider_; }
    Collider::ColliderID GetColliderID() const { return colliderId_; }

    void SetCollider(Collider *collider) { collider_ = collider; }

#if defined(USE_IMGUI)
    void ShowImGui() override {}
#endif

private:
    Collider *collider_ = nullptr;
    ColliderInfo2D info_{};
    Collider::ColliderID colliderId_ = 0;
};

} // namespace KashipanEngine
