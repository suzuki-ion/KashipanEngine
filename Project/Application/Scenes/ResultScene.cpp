#include "Scenes/ResultScene.h"

#include "Core/Window.h"
#include "Scenes/Components/SceneChangeIn.h"
#include "Scenes/Components/SceneChangeOut.h"

#include "Objects/SystemObjects/Camera2D.h"
#include "Objects/SystemObjects/Camera3D.h"
#include "Objects/SystemObjects/DirectionalLight.h"

#include "Objects/Components/2D/Transform2D.h"
#include "Objects/Components/2D/Material2D.h"
#include "Objects/GameObjects/2D/Sprite.h"

#include "Objects/Components/3D/Transform3D.h"

namespace KashipanEngine {

ResultScene::ResultScene()
    : SceneBase("ResultScene") {

    auto *defaultVariables = GetSceneComponent<SceneDefaultVariables>();
    auto *screenBuffer3D = defaultVariables ? defaultVariables->GetScreenBuffer3D() : nullptr;
    
    if (screenBuffer3D) {
        ChromaticAberrationEffect::Params p{};
        p.directionX = 1.0f;
        p.directionY = 0.0f;
        p.strength = 0.001f;
        screenBuffer3D->RegisterPostEffectComponent(std::make_unique<ChromaticAberrationEffect>(p));

        BloomEffect::Params bp{};
        bp.threshold = 1.0f;
        bp.softKnee = 0.25f;
        bp.intensity = 0.5f;
        bp.blurRadius = 1.0f;
        bp.iterations = 4;
        screenBuffer3D->RegisterPostEffectComponent(std::make_unique<BloomEffect>(bp));
    }

    AddSceneComponent(std::make_unique<SceneChangeIn>());
    AddSceneComponent(std::make_unique<SceneChangeOut>());

    if (auto *sceneChangeIn = GetSceneComponent<SceneChangeIn>()) {
        sceneChangeIn->Play();
    }
}

ResultScene::~ResultScene() {
}

void ResultScene::OnUpdate() {
    // SceneChangeOut 完了で次シーンへ
    if (!GetNextSceneName().empty()) {
        if (auto *sceneChangeOut = GetSceneComponent<SceneChangeOut>()) {
            if (sceneChangeOut->IsFinished()) {
                ChangeToNextScene();
            }
        }
    }
}

} // namespace KashipanEngine
