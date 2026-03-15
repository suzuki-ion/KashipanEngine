#include "Scenes/ResultScene.h"
#include "Scenes/Components/SceneChangeIn.h"
#include "Scenes/Components/SceneChangeOut.h"
#include "Scenes/Components/ResultSceneAnimator.h"
#include "Scenes/Components/BackgroundSprite.h"

#include <MatsumotoUtility.h>

using namespace Application::MatsumotoUtility;

namespace KashipanEngine {

ResultScene::ResultScene()
    : SceneBase("ResultScene") {
}

void ResultScene::Initialize() {
    sceneDefaultVariables_ = GetSceneComponent<SceneDefaultVariables>();

    AddSceneComponent(std::make_unique<BackgroundSprite>());
    AddSceneComponent(std::make_unique<SceneChangeIn>());
    AddSceneComponent(std::make_unique<SceneChangeOut>());
    AddSceneComponent(std::make_unique<ResultSceneAnimator>());

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

    {
        auto text = std::make_unique<Text>(64);
        text->SetName("WinnerText");
        text->SetFont("Assets/Application/test.fnt");
        text->SetTextAlign(TextAlignX::Center, TextAlignY::Center);
        if (auto* tr = text->GetComponent2D<Transform2D>()) {
            tr->SetTranslate(Vector3(960.0f, 260.0f, 0.0f));
        }
        if (sceneDefaultVariables_ && sceneDefaultVariables_->GetScreenBuffer2D()) {
            text->AttachToRenderer(sceneDefaultVariables_->GetScreenBuffer2D(), "Object2D.DoubleSidedCulling.BlendNormal");
        } else if (sceneDefaultVariables_ && sceneDefaultVariables_->GetMainWindow()) {
            text->AttachToRenderer(sceneDefaultVariables_->GetMainWindow(), "Object2D.DoubleSidedCulling.BlendNormal");
        }

        const int winner = GetSceneVariableOr<int>("PuzzleWinner", 0);
        if (winner == 1) {
            text->SetText("Player 1 Wins!");
        } else if (winner == 2) {
            text->SetText("Player 2 Wins!");
        } else {
            text->SetText("このテキストは表示されないはずだよ");
        }

        winnerText_ = text.get();
        AddObject2D(std::move(text));
    }

    resultSceneAnimator_ = GetSceneComponent<ResultSceneAnimator>();
    if (resultSceneAnimator_) {
        std::vector<Transform2D*> transforms;
        if (auto* bg = spriteMap_["BackToTitle"]) {
            if (auto* tr = bg->GetComponent2D<Transform2D>()) {
                transforms.push_back(tr);
                resultSceneAnimator_->SetBackgroundTransform(tr);
            }
        }
        resultSceneAnimator_->SetTargetTransforms(std::move(transforms));
        resultSceneAnimator_->PlayBackgroundDropIn();
    }

    const auto bgmHandle = AudioManager::GetSoundHandleFromFileName("bgmResult.mp3");
    if (bgmHandle != AudioManager::kInvalidSoundHandle) {
        AudioManager::PlayParams p{};
        p.sound = bgmHandle;
        p.volume = 0.3f;
        p.loop = true;
        bgmPlayer_.AddAudio(p);
        bgmPlayer_.ChangeAudio(0.0, 0);
    }

    prevSelectedNumber_ = resultSelector_.GetSelectNumber();
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

        if (selectedNumber != prevSelectedNumber_) {
            auto selectHandle = AudioManager::GetSoundHandleFromFileName("menuSelect.mp3");
            if (selectHandle != AudioManager::kInvalidSoundHandle) {
                AudioManager::Play(selectHandle, 1.0f, 0.0f, false);
            }
            prevSelectedNumber_ = selectedNumber;
        }

        if (selectedNumber == 0) {
            SetTextureToSprite(spriteMap_["BackToTitle"], "result_0.png");
        }
        else if (selectedNumber == 1) {
            SetTextureToSprite(spriteMap_["BackToTitle"], "result_1.png");
        }
    }
    


    if (resultSelector_.IsSelecting() && GetNextSceneName().empty()) {
		int selectedNumber = resultSelector_.GetSelectNumber(); 
        if (selectedNumber == 0) {
            SetNextSceneName("TitleScene");
        }
        else if (selectedNumber == 1) {
            SetNextSceneName("GameScene");
        }

        auto decideHandle = AudioManager::GetSoundHandleFromFileName("menuDecide.mp3");
        if (decideHandle != AudioManager::kInvalidSoundHandle) {
            AudioManager::Play(decideHandle, 1.0f, 0.0f, false);
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
