#pragma once
#include <KashipanEngine.h>
#include "Objects/Components/PlayerCollisionPushBack.h"
#include "Objects/Components/PlayerInputHandler.h"
#include "Objects/Components/PlayerMovement.h"

namespace KashipanEngine {

std::unique_ptr<Object3DBase> CreatePlayerObject(SceneContext *context) {
    auto *sceneDefaultVariables = context->GetComponent<SceneDefaultVariables>();
    auto *screenBuffer3D = sceneDefaultVariables ? sceneDefaultVariables->GetScreenBuffer3D() : nullptr;

    auto player = std::make_unique<Sphere>();
    player->SetName("Player");
    if (auto *tr = player->GetComponent3D<Transform3D>()) {
        tr->SetTranslate(Vector3(0.0f, 2.0f, 0.0f));
        tr->SetScale(Vector3(1.0f, 1.0f, 1.0f));
    }
    player->RegisterComponent(std::make_unique<RigidBody3D>());
    player->RegisterComponent(std::make_unique<PlayerInputHandler>());
    player->RegisterComponent(std::make_unique<PlayerCollisionPushBack>());
    player->RegisterComponent(std::make_unique<PlayerMovement>());

    player->AttachToRenderer(screenBuffer3D, "Object3D.Solid.BlendNormal");

    return std::move(player);
}

} // namespace KashipanEngine