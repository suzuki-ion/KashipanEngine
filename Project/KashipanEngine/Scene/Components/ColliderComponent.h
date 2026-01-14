#pragma once

#include "Objects/Collision/Collider.h"
#include "Scene/Components/ISceneComponent.h"
#include "Scene/SceneContext.h"

namespace KashipanEngine {

class ColliderComponent final : public ISceneComponent {
public:
    ColliderComponent() : ISceneComponent("ColliderComponent") {}
    ~ColliderComponent() override = default;

    ColliderComponent(const ColliderComponent &) = delete;
    ColliderComponent &operator=(const ColliderComponent &) = delete;

    Collider *GetCollider() { return &collider_; }
    const Collider *GetCollider() const { return &collider_; }

    void Update() override {
        collider_.Update2D();
        collider_.Update3D();
    }

    void Finalize() override {
        collider_.Clear2D();
        collider_.Clear3D();
    }

private:
    Collider collider_{};
};

} // namespace KashipanEngine
