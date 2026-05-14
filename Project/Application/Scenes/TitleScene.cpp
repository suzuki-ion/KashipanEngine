#include "Scenes/TitleScene.h"
#include "Scenes/Components/SceneChangeIn.h"
#include "Scenes/Components/SceneChangeOut.h"
#include "Objects/GameObjects/3D/Skybox.h"
#include "Assets/TextureManager.h"
#include "Objects/Components/3D/Material3D.h"
#include "Objects/Components/3D/Transform3D.h"

namespace KashipanEngine {

TitleScene::TitleScene()
    : SceneBase("TitleScene") {}

void TitleScene::Initialize() {
    sceneDefaultVariables_ = GetSceneComponent<SceneDefaultVariables>();
    auto *mainCamera3D = sceneDefaultVariables_ ? sceneDefaultVariables_->GetMainCamera3D() : nullptr;
    auto *screenBuffer3D = sceneDefaultVariables_ ? sceneDefaultVariables_->GetScreenBuffer3D() : nullptr;

    AddSceneComponent(std::make_unique<SceneChangeIn>());
    AddSceneComponent(std::make_unique<SceneChangeOut>());
    AddSceneComponent(std::make_unique<ParticleManager>());

    if (auto *in = GetSceneComponent<SceneChangeIn>()) {
        in->Play();
    }

    AddSceneComponent(std::make_unique<ParticleManager>());
    if (mainCamera3D) {
        auto debugCameraMovement = std::make_unique<DebugCameraMovement>(mainCamera3D);
        debugCameraMovement->SetEnable(true);
        AddSceneComponent(std::move(debugCameraMovement));
    }

    // Create test skybox
    if (screenBuffer3D) {
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
        AddObject3D(std::move(skybox));
    }
}

TitleScene::~TitleScene() {}

void TitleScene::OnUpdate() {
    if (!GetNextSceneName().empty()) {
        if (auto *out = GetSceneComponent<SceneChangeOut>()) {
            if (out->IsFinished()) {
                ChangeToNextScene();
            }
        }
    }

    if (auto *ic = GetInputCommand()) {
        if (ic->Evaluate("DebugSceneChange").Triggered()) {
            if (GetNextSceneName().empty()) {
                SetNextSceneName("MenuScene");
            }
            if (auto *out = GetSceneComponent<SceneChangeOut>()) {
                out->Play();
            }
        }
    }
}

} // namespace KashipanEngine
