#pragma once

#include "Objects/Collision/Collider.h"
#include "Scene/Components/SceneComponentHeader.h"

namespace KashipanEngine {

class ColliderComponent final : public ISceneComponent {
public:
    SCENE_COMPONENT_CONSTRUCTOR(ColliderComponent, 1, )
    ~ColliderComponent() override = default;

    std::unique_ptr<ISceneComponent> Clone() const override {
        return std::make_unique<ColliderComponent>();
    }

    Collider *GetCollider() { return &collider_; }
    const Collider *GetCollider() const { return &collider_; }

protected:
    void Update() override {
        collider_.Update2D();
        // ReactPhysics3D is updated in this existing call.
        collider_.Update3D();
    }

    void Finalize() override {
        collider_.Clear2D();
        collider_.Clear3D();
    }

private:
    Collider collider_{};
};

REGISTER_COMPONENT_SCENE(ColliderComponent)

} // namespace KashipanEngine
