#pragma once
#include <KashipanEngine.h>
#include "Objects/Components/PlayerInputHandler.h"

namespace KashipanEngine {

std::unique_ptr<Object3DBase> CreatePlayerObject(SceneContext *context) {
    auto *sceneDefaultVariables = context->GetComponent<SceneDefaultVariables>();
    auto *screenBuffer3D = sceneDefaultVariables ? sceneDefaultVariables->GetScreenBuffer3D() : nullptr;

    auto player = std::make_unique<Sphere>();
    player->SetName("Player");
    player->RegisterComponent(std::make_unique<PlayerInputHandler>());
    player->AttachToRenderer(screenBuffer3D, "Object3D.Solid.BlendNormal");

    return std::move(player);
}

} // namespace KashipanEngine