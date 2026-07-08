#include "ICollider.h"

#include "Objects/Components/Transform.h"
#include "Objects/ObjectContext.h"
#include "Scene/Components/SceneObjectCollider.h"
#include "Scene/SceneContext.h"

namespace KashipanEngine {

void ICollider::Initialize() {
    auto *sceneContext = GetOwnerSceneContext();
    if (!sceneContext) return;
    auto *sceneObjectCollider = sceneContext->GetComponent<SceneObjectCollider>();
    if (!sceneObjectCollider) {
        sceneObjectCollider = sceneContext->AddComponent<SceneObjectCollider>();
    }
    if (sceneObjectCollider) {
        sceneObjectCollider->RegisterCollider(this);
    }
}

void ICollider::Finalize() {
    auto *sceneContext = GetOwnerSceneContext();
    auto *sceneObjectCollider = sceneContext ? sceneContext->GetComponent<SceneObjectCollider>() : nullptr;
    if (sceneObjectCollider) {
        sceneObjectCollider->UnregisterCollider(this);
    }
}

Vector3 ICollider::GetOwnerWorldPosition() const {
    auto *objectContext = GetOwnerObjectContext();
    auto *transform = objectContext ? objectContext->GetComponent<Transform>() : nullptr;
    if (!transform) return Vector3(0.0f, 0.0f, 0.0f);
    const Matrix4x4 &world = transform->GetWorldMatrix();
    return Vector3(world.m[3][0], world.m[3][1], world.m[3][2]);
}

} // namespace KashipanEngine
