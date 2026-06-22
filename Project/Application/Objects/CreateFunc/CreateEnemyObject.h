#pragma once
#include <KashipanEngine.h>
#include "Objects/Components/EnemyCollision.h"
#include "Objects/Components/EnemyAliveStateController.h"

namespace KashipanEngine {

std::unique_ptr<Object3DBase> CreateEnemyObject(SceneContext *context, const Vector3 &position = Vector3::Zero()) {
    auto *sceneDefaultVariables = context->GetComponent<SceneDefaultVariables>();
    auto *screenBuffer3D = sceneDefaultVariables ? sceneDefaultVariables->GetScreenBuffer3D() : nullptr;

    auto enemy = std::make_unique<Sphere>();
    enemy->SetName("Enemy");
    enemy->RegisterComponent(std::make_unique<EnemyCollision>());
    enemy->RegisterComponent(std::make_unique<EnemyAliveStateController>());
    if (auto *tr = enemy->GetComponent3D<Transform3D>()) {
        tr->SetTranslate(position);
        tr->SetScale(Vector3(1.0f, 1.0f, 1.0f));
    }
    if (auto *mat = enemy->GetComponent3D<Material3D>()) {
        mat->SetColor(Vector4(1.0f, 0.0f, 0.0f, 1.0f));
    }
    enemy->AttachToRenderer(screenBuffer3D, "Object3D.Solid.BlendNormal");

    return std::move(enemy);
}

} // namespace KashipanEngine