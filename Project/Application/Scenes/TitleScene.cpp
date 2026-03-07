#include "Scenes/TitleScene.h"
#include "Scenes/Components/SceneChangeIn.h"
#include "Scenes/Components/SceneChangeOut.h"

#include <MatsumotoUtility.h>

namespace KashipanEngine {

TitleScene::TitleScene()
    : SceneBase("TitleScene") {
}

void TitleScene::Initialize() {
    sceneDefaultVariables_ = GetSceneComponent<SceneDefaultVariables>();

    AddSceneComponent(std::make_unique<SceneChangeIn>());
    AddSceneComponent(std::make_unique<SceneChangeOut>());

    if (auto *in = GetSceneComponent<SceneChangeIn>()) {
        in->Play();
    }

	transitionStarted_ = false;

	// タイトルセレクトマネージャーの初期化
	titleSelectManager_.Initialize(
        [this]() {return GetInputCommand()->Evaluate("Up").Triggered(); },
        [this]() {return GetInputCommand()->Evaluate("Down").Triggered(); },
        [this]() {return GetInputCommand()->Evaluate("Submit").Triggered(); },
        [this]() {return GetInputCommand()->Evaluate("Cancel").Triggered(); },
        [this]() {return GetInputCommand()->Evaluate("P2Submit").Triggered(); },
        [this]() {return GetInputCommand()->Evaluate("P2Cancel").Triggered(); }
    );

	// スプライトの生成関数
    auto CreateSirite =
        [this](const std::string& name)
        { return Application::MatsumotoUtility::CreateSpriteObject(
            sceneDefaultVariables_->GetScreenBuffer2D(),
            [this](std::unique_ptr<Object2DBase> obj) { return AddObject2D(std::move(obj)); },
            name); };
	// タイトルスプライトマネージャーの初期化
	titleSpriteManager_.Initialize(CreateSirite);

}

TitleScene::~TitleScene() {
}

void TitleScene::OnUpdate() {
	// タイトルセレクトマネージャーの更新
	float deltaTime = KashipanEngine::GetDeltaTime();
    titleSelectManager_.Update(deltaTime);
	titleSpriteManager_.Update(
        deltaTime,titleSelectManager_.GetCurrentSection(),titleSelectManager_.GetCurrentSelectNumber());

	// モード選択が完了して遷移すべきか
    if (titleSelectManager_.GetModeSelectSubmitted()) {
        if (!transitionStarted_) {
            if (auto* out = GetSceneComponent<SceneChangeOut>()) {
                out->Play();
				transitionStarted_ = true;
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

    if (!GetNextSceneName().empty()) {
        if (auto *out = GetSceneComponent<SceneChangeOut>()) {
            if (out->IsFinished()) {
                // 遷移先はゲームシーン
                SetNextSceneName("TestScene");
                ChangeToNextScene();
            }
        }
    }
}

} // namespace KashipanEngine
