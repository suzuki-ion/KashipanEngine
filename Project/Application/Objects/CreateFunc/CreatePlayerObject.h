#pragma once
#include <KashipanEngine.h>
#include "Objects/Components/PlayerCollision.h"
#include "Objects/Components/PlayerInputHandler.h"
#include "Objects/Components/PlayerMovement.h"
#include "Objects/Components/PlayerEnemyJump.h"

namespace KashipanEngine {

std::unique_ptr<EmptyObject> CreatePlayerObject(SceneContext *context) {
    auto *sceneDefaultVariables = context->GetComponent<SceneDefaultVariables>();
    auto *screenBuffer3D = sceneDefaultVariables ? sceneDefaultVariables->GetScreenBuffer3D() : nullptr;

    auto player = std::make_unique<Sphere>();
    player->SetName("Player");
    player->RegisterComponent(std::make_unique<PlayerEnemyJump>());
    player->RegisterComponent(std::make_unique<PlayerMovement>());
    player->RegisterComponent(std::make_unique<PlayerCollision>());
    player->RegisterComponent(std::make_unique<PlayerInputHandler>());
    if (auto *tr = player->GetComponent3D<Transform3D>()) {
        tr->SetTranslate(Vector3(-4.0f, 6.0f, 0.0f));
        tr->SetScale(Vector3(1.0f, 1.0f, 1.0f));
    }

    player->AttachToRenderer(screenBuffer3D, "Object.Solid.BlendNormal");

    return std::move(player);
}

} // namespace KashipanEngine