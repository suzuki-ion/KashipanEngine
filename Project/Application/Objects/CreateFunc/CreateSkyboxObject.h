#pragma once
#include <KashipanEngine.h>

namespace KashipanEngine {

std::unique_ptr<EmptyObject> CreateSkyboxObject(SceneContext *context) {
    auto *sceneDefaultVariables = context->GetComponent<SceneDefaultVariables>();
    auto *screenBuffer3D = sceneDefaultVariables ? sceneDefaultVariables->GetScreenBuffer3D() : nullptr;

    auto skybox = std::make_unique<Skybox>();
    skybox->SetName("TestSkybox");
    if (auto *material = skybox->GetComponent3D<Material3D>()) {
        auto cubemapTexture = TextureManager::GetTextureFromFileName("rostock_laage_airport_4k.dds");
        material->SetTexture(cubemapTexture);
    }
    if (auto *transform = skybox->GetComponent3D<Transform3D>()) {
        transform->SetScale(Vector3(100.0f, 100.0f, 100.0f));
    }
    skybox->AttachToRenderer(screenBuffer3D, "Skybox.Solid.BlendNormal");

    return std::move(skybox);
}
} // namespace KashipanEngine