#include "PreTransform.h"

#include "Objects/Components/Transform.h"
#include "Objects/ObjectContext.h"
#include "Scene/Components/ScenePreTransform.h"
#include "Scene/SceneContext.h"

namespace KashipanEngine {

void PreTransform::CaptureCurrentAsPrevious() {
    auto *objectContext = GetOwnerObjectContext();
    auto *transform = objectContext ? objectContext->GetComponent<Transform>() : nullptr;
    if (!transform) return;
    previousTranslate_ = transform->GetTranslate();
    previousRotate_ = transform->GetRotate();
    previousRotateQuaternion_ = transform->GetRotateQuaternion();
    previousScale_ = transform->GetScale();
    previousWorldMatrix_ = transform->GetWorldMatrix();
    previousWorldPosition_ = transform->GetWorldPosition();
    previousWorldRotate_ = transform->GetWorldRotate();
    previousWorldRotateQuaternion_ = transform->GetWorldRotateQuaternion();
    previousWorldScale_ = transform->GetWorldScale();
}

void PreTransform::Initialize() {
    auto *sceneContext = GetOwnerSceneContext();
    if (!sceneContext) return;
    auto *scenePreTransform = sceneContext->GetComponent<ScenePreTransform>();
    if (!scenePreTransform) {
        scenePreTransform = sceneContext->AddComponent<ScenePreTransform>();
    }
    if (scenePreTransform) {
        scenePreTransform->RegisterPreTransform(this);
    }
}

void PreTransform::Finalize() {
    auto *sceneContext = GetOwnerSceneContext();
    auto *scenePreTransform = sceneContext ? sceneContext->GetComponent<ScenePreTransform>() : nullptr;
    if (scenePreTransform) {
        scenePreTransform->UnregisterPreTransform(this);
    }
}

} // namespace KashipanEngine
