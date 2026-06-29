#pragma once
#include <KashipanEngine.h>
#include "Objects/CollisionAttributes.h"

namespace KashipanEngine {

std::unique_ptr<EmptyObject> CreateGroundObject(SceneContext *context) {
    auto *sceneDefaultVariables = context->GetComponent<SceneDefaultVariables>();
    auto *screenBuffer3D = sceneDefaultVariables ? sceneDefaultVariables->GetScreenBuffer3D() : nullptr;

    auto ground = std::make_unique<Box>();
    ground->SetName("Ground");
    ColliderInfo3D colliderInfo{};
    ColliderInfo3D::BoxShape3D groundBox{};
    groundBox.center = Vector3(0.0f, 0.0f, 0.0f);
    groundBox.halfExtents = Vector3(0.5f, 0.5f, 0.5f);
    colliderInfo.shape = groundBox;
    colliderInfo.attribute = CollisionAttribute::Ground;
    colliderInfo.ignoreAttribute = CollisionAttribute::Ground;
    ground->RegisterComponent(std::make_unique<Collision3D>(colliderInfo));
    if (auto *tr = ground->GetComponent3D<Transform3D>()) {
        tr->SetTranslate(Vector3(0.0f, 0.0f, 0.0f));
        tr->SetScale(Vector3(256.0f, 1.0f, 1.0f));
        tr->SetRotate(Vector3(0.0f, 0.0f, ToRadians(30.0f)));
    }
    ground->AttachToRenderer(screenBuffer3D, "Object.Solid.BlendNormal");

    return std::move(ground);
}

} // namespace KashipanEngine