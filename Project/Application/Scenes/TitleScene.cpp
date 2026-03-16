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

    // ============================================================
    // ユーティリティの生成
    // ============================================================
    // ゲームにスプライトを生成、追加する関数
    createSpriteFunction_ =
        [this](const std::string& name) {
        return Application::MatsumotoUtility::CreateSpriteObject(
            sceneDefaultVariables_->GetScreenBuffer2D(),
            [this](std::unique_ptr<Object2DBase> obj) { return AddObject2D(std::move(obj)); },
            name);
        };
    // ゲームにスプライトを特定のテクスチャで生成、追加する関数
    createSpriteWithTextureFunction_ =
        [this](const std::string& name, const std::string& textureName) {
        KashipanEngine::Sprite* sprite = createSpriteFunction_(name);
        Application::MatsumotoUtility::SetTextureToSprite(sprite, textureName);
        Application::MatsumotoUtility::FitSpriteToTexture(sprite);
        return sprite;
        };
    // 画面の中心
    if (auto* window = sceneDefaultVariables_->GetMainWindow()) {
        screenCenter_ = Vector2(static_cast<float>(window->GetClientWidth()) * 0.5f, static_cast<float>(window->GetClientHeight()) * 0.5f);
    }

    // ============================================================
    // ゲームで使うオブジェクトたちの生成
    // ============================================================

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

	// タイトルスプライトマネージャーの初期化
	titleSpriteManager_.Initialize(createSpriteFunction_);
    SetNextSceneName("GameScene");

    // ================================================================
    // BGM
    // ================================================================
    {
        AudioManager::PlayParams params;
        params.sound = AudioManager::GetSoundHandleFromFileName("bgmTitle.mp3");
        params.volume = 0.2f;
        params.loop = true;
        audioPlayer_.AddAudio(params);
        audioPlayer_.ChangeAudio(2.0);
    }
}

TitleScene::~TitleScene() {
}

void TitleScene::OnUpdate() {
    if (!GetNextSceneName().empty()) {
        if (auto* out = GetSceneComponent<SceneChangeOut>()) {
            if (out->IsFinished()) {
                ChangeToNextScene();
            }
        }
    }

	// タイトルセレクトマネージャーの更新
	float deltaTime = KashipanEngine::GetDeltaTime();
    titleSelectManager_.Update(deltaTime);
	titleSpriteManager_.SetTriggered1PTimer(titleSelectManager_.Get1PTriggerGraceTime());
	titleSpriteManager_.SetTriggered2PTimer(titleSelectManager_.Get2PTriggerGraceTime());
	titleSpriteManager_.Update(
        deltaTime,titleSelectManager_.GetCurrentSection(),titleSelectManager_.GetCurrentSelectNumber());

	// モード選択が完了して遷移すべきか
    if (titleSelectManager_.GetModeSelectSubmitted()) {
        Application::TitleSection selectedSection = titleSelectManager_.GetCurrentSection();
        if (selectedSection == Application::TitleSection::AISelect) {
            int aiNumber = titleSelectManager_.GetCurrentSelectNumber();
            AddSceneVariable("SelectedAINumber", aiNumber);
        } else if (selectedSection == Application::TitleSection::MultiplayerSelect) {
            int multiplayerNumber = titleSelectManager_.GetCurrentSelectNumber();
            AddSceneVariable("SelectedMultiplayerNumber", multiplayerNumber);
        }
        AddSceneVariable("SelectedModeSection", selectedSection);
        if (auto* out = GetSceneComponent<SceneChangeOut>()) {
            out->Play();
            transitionStarted_ = true;
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
