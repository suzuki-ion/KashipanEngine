#include "Scenes/ResultScene.h"
#include "Scenes/Components/SceneChangeIn.h"
#include "Scenes/Components/SceneChangeOut.h"

#include <MatsumotoUtility.h>

using namespace Application::MatsumotoUtility;

namespace KashipanEngine {

ResultScene::ResultScene()
    : SceneBase("ResultScene") {
}

void ResultScene::Initialize() {
    sceneDefaultVariables_ = GetSceneComponent<SceneDefaultVariables>();

    AddSceneComponent(std::make_unique<SceneChangeIn>());
    AddSceneComponent(std::make_unique<SceneChangeOut>());

    if (auto *in = GetSceneComponent<SceneChangeIn>()) {
        in->Play();
    }

	// 結果セレクタの初期化
	resultSelector_.Initialize(
        [this]() {return GetInputCommand()->Evaluate("Up").Triggered(); },
        [this]() {return GetInputCommand()->Evaluate("Down").Triggered(); },
        [this]() {return GetInputCommand()->Evaluate("Submit").Triggered(); },
        [this]() {return GetInputCommand()->Evaluate("Cancel").Triggered(); }
    );

    auto CreateSirite =
        [this](const std::string& name)
        { return Application::MatsumotoUtility::CreateSpriteObject(
            sceneDefaultVariables_->GetScreenBuffer2D(),
            [this](std::unique_ptr<Object2DBase> obj) { return AddObject2D(std::move(obj)); },
            name); };

	spriteMap_["BackToTitle"] = CreateSirite("result");
	SetTextureToSprite(spriteMap_["BackToTitle"], "result_0.png");
	FitSpriteToTexture(spriteMap_["BackToTitle"]);
	SetTranslateToSprite(spriteMap_["BackToTitle"], Vector3(960.0f, 540.0f, 0.0f));
}

ResultScene::~ResultScene() {
}

void ResultScene::OnUpdate() {

    if (!GetNextSceneName().empty()) {
        if (auto* out = GetSceneComponent<SceneChangeOut>()) {
            if (out->IsFinished()) {
                ChangeToNextScene();
            }
        }
    }

    // 結果セレクタの更新
    resultSelector_.Update();
    {
        int selectedNumber = resultSelector_.GetSelectNumber();
        if (selectedNumber == 0) {
            SetTextureToSprite(spriteMap_["BackToTitle"], "result_0.png");
        }
        else if (selectedNumber == 1) {
            SetTextureToSprite(spriteMap_["BackToTitle"], "result_1.png");
        }
    }
    


    if (resultSelector_.IsSelecting()) {
		int selectedNumber = resultSelector_.GetSelectNumber(); 
        if (selectedNumber == 0) {
            SetNextSceneName("TitleScene");
        }
        else if (selectedNumber == 1) {
            SetNextSceneName("TestScene");
        }

        if (auto* out = GetSceneComponent<SceneChangeOut>()) {
            out->Play();
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
