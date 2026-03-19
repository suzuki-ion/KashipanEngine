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

    AddSceneComponent(std::make_unique<SceneChangeIn>());
    AddSceneComponent(std::make_unique<SceneChangeOut>());

    if (auto *in = GetSceneComponent<SceneChangeIn>()) {
        in->Play();
    }

	// =============================================================
	// ユーティリティの生成
	// =============================================================
    // ゲームにスプライトを生成、追加する関数
    createSpriteFunction_ =
        [this](const std::string& name, KashipanEngine::DefaultSampler defaultSampler) {
        return Application::MatsumotoUtility::CreateSpriteObject(
            sceneDefaultVariables_->GetScreenBuffer2D(),
            [this](std::unique_ptr<Object2DBase> obj) { return AddObject2D(std::move(obj)); },
            name,
            defaultSampler);
        };
    // ゲームにスプライトを特定のテクスチャで生成、追加する関数
    createSpriteWithTextureFunction_ =
        [this](const std::string& name, const std::string& textureName, KashipanEngine::DefaultSampler defaultSampler) {
        KashipanEngine::Sprite* sprite = createSpriteFunction_(name, defaultSampler);
        Application::MatsumotoUtility::SetTextureToSprite(sprite, textureName);
        Application::MatsumotoUtility::FitSpriteToTexture(sprite);
        return sprite;
        };
	// 画面の中心
    if (auto *screenBuffer2D = sceneDefaultVariables_->GetScreenBuffer2D()) {
        screenCenter_ = Vector2(static_cast<float>(screenBuffer2D->GetWidth()) * 0.5f, static_cast<float>(screenBuffer2D->GetHeight()) * 0.5f);
    }
	// タイマー
	timer_ = 0.0f;

    // =============================================================
    // ゲームで使うスプライトの生成
    // =============================================================
	float cutinBarOffsetR = 0.05f; // カットインバーの斜めのオフセット 
    
    // リザルト画面の背景
	spriteMap_["Background"] = createSpriteWithTextureFunction_("Background", "whiteBG.png", KashipanEngine::DefaultSampler::LinearWrap);
    SetTranslateToSprite(spriteMap_["Background"], Vector3(screenCenter_.x, screenCenter_.y, 0.0f));

	// 上のカットインバー
	spriteMap_["CutInBarUp"] = createSpriteWithTextureFunction_("CutInBar", "cutInBar.png", KashipanEngine::DefaultSampler::LinearWrap);
	SetTranslateToSprite(spriteMap_["CutInBarUp"], Vector3(screenCenter_.x, screenCenter_.y, 0.0f));
	ScaleSprite(spriteMap_["CutInBarUp"], 1.1f);
	SetRotationToSprite(spriteMap_["CutInBarUp"], Vector3(0.0f, 0.0f, 3.14f+ cutinBarOffsetR));

    // 勝利プレイヤーの親スプライト
	spriteMap_["WinnerPlayer"] = createSpriteFunction_("WinnerPlayer", KashipanEngine::DefaultSampler::LinearClamp);

    // 勝利プレイヤーの頭後ろ
    spriteMap_["WinnerHeadBack"] = createSpriteWithTextureFunction_("WinnerHeadBack", "result_headBack_1.png", KashipanEngine::DefaultSampler::LinearClamp);
	
    // 勝利プレイヤーの左腕
    spriteMap_["WinnerArmL"] = createSpriteWithTextureFunction_("WinnerArmL", "result_leftArm_1.png", KashipanEngine::DefaultSampler::LinearClamp);
    // 勝利プレイヤーの胴
	spriteMap_["WinnerBody"] = createSpriteWithTextureFunction_("WinnerBody", "result_body_1.png", KashipanEngine::DefaultSampler::LinearClamp);
	// 勝利プレイヤーの頭
    spriteMap_["WinnerHead"] = createSpriteWithTextureFunction_("WinnerHead", "result_head_1.png", KashipanEngine::DefaultSampler::LinearClamp);

    // 勝利者の表示
	spriteMap_["WinnerText"] = createSpriteWithTextureFunction_("WinnerText", "result_Win0.png", KashipanEngine::DefaultSampler::LinearClamp);
	SetTranslateToSprite(spriteMap_["WinnerText"], Vector3(screenCenter_.x * 2.0f, screenCenter_.y , 0.0f));

    // 下のカットインバー
    spriteMap_["CutInBarDown"] = createSpriteWithTextureFunction_("CutInBar", "cutInBar.png", KashipanEngine::DefaultSampler::LinearWrap);
    SetTranslateToSprite(spriteMap_["CutInBarDown"], Vector3(screenCenter_.x, screenCenter_.y, 0.0f));
    ScaleSprite(spriteMap_["CutInBarDown"], 1.1f);
    SetRotationToSprite(spriteMap_["CutInBarDown"], Vector3(0.0f, 0.0f, cutinBarOffsetR));

    // 勝利プレイヤーの右腕
    spriteMap_["WinnerArmR"] = createSpriteWithTextureFunction_("WinnerArmR", "result_rightArm_1.png", KashipanEngine::DefaultSampler::LinearClamp);
	
    // 結果セレクタの選択肢スプライトを初期化
    spriteMap_["BackToTitle"] = createSpriteWithTextureFunction_("result", "result_0.png", KashipanEngine::DefaultSampler::LinearClamp);
    SetTranslateToSprite(spriteMap_["BackToTitle"], Vector3(screenCenter_.x, screenCenter_.y, 0.0f));
	SetScaleToSprite(spriteMap_["BackToTitle"], Vector3(0.0f, 0.0f, 0.0f));

	// =============================================================
	// ゲームで使うオブジェクトたちの生成
    // =============================================================
    // 結果セレクタの初期化
    resultSelector_.Initialize(
        [this]() {return GetInputCommand()->Evaluate("Up").Triggered(); },
        [this]() {return GetInputCommand()->Evaluate("Down").Triggered(); },
        [this]() {return GetInputCommand()->Evaluate("Submit").Triggered(); },
        [this]() {return GetInputCommand()->Evaluate("Cancel").Triggered(); }
    );
	// 結果の画像から一回ボタンを押させるためのフラグ
	isReadyToSelect_ = false;

	// 勝利したプレイヤーの番号（0または1）を管理する変数（-1は未設定）
	winnerPlayerNumber_ = 0;
    isNpcMode_ = false;
    
    // =============================================================
    // BGMの再生
    // =============================================================
    const auto bgmHandle = AudioManager::GetSoundHandleFromFileName("bgmResult.mp3");
    if (bgmHandle != AudioManager::kInvalidSoundHandle) {
        AudioManager::PlayParams p{};
        p.sound = bgmHandle;
        p.volume = 0.3f;
        p.loop = true;
        bgmPlayer_.AddAudio(p);
        bgmPlayer_.ChangeAudio(0.0, 0);
    }

    // ================================================================
    // スプライトアニメーター
    // ================================================================
    {
        auto cmp = std::make_unique<SpriteAnimator>();
        auto *cmpP = cmp.get();
        AddSceneComponent(std::move(cmp));
        cmpP->LoadFromJsonFile("WinnerPlayer1.json");
        cmpP->Play("WinnerPlayer1");
    }

    prevSelectedNumber_ = resultSelector_.GetSelectNumber();
}

ResultScene::~ResultScene() {
}

void ResultScene::OnUpdate() {
	float deltaTime = GetDeltaTime();
	timer_ += deltaTime;

    if (!GetNextSceneName().empty()) {
        if (auto* out = GetSceneComponent<SceneChangeOut>()) {
            if (out->IsFinished()) {
                ChangeToNextScene();
            }
        }
    }

	// 画面の中心を更新
    if (auto *screenBuffer2D = sceneDefaultVariables_->GetScreenBuffer2D()) {
        screenCenter_ = Vector2(static_cast<float>(screenBuffer2D->GetWidth()) * 0.5f, static_cast<float>(screenBuffer2D->GetHeight()) * 0.5f);
    }

	// 背景のスクロール
	MoveTextureUVToSprite(spriteMap_["Background"], Vector2(0.0f, deltaTime));

    // カットインバーのスクロール
	float cutInBarScrollSpeed = 10.0f; // カットインバーのスクロール速度
	MoveTextureUVToSprite(spriteMap_["CutInBarUp"], Vector2(cutInBarScrollSpeed * deltaTime, 0.0f));
	MoveTextureUVToSprite(spriteMap_["CutInBarDown"], Vector2(cutInBarScrollSpeed * deltaTime, 0.0f));

	// 最初の1秒間は何も表示せずに待つ
    if (timer_ < 1.0f) {
		return;
    }

	// 勝利者の文字を出現させる
	SimpleEaseSpriteMove(spriteMap_["WinnerText"], Vector3(screenCenter_.x, screenCenter_.y + cosf(timer_) * 5.0f, 0.0f), 0.2f);
    // 勝利者のスプライトを変化させる
    SetTextureToSprite(spriteMap_["WinnerText"], winnerPlayerNumber_ == 0 ? "result_Win0.png" : "result_Win1.png");

    // 結果セレクタの更新
    if (isReadyToSelect_) {
        resultSelector_.Update();
        int selectedNumber = resultSelector_.GetSelectNumber();

        if (selectedNumber != prevSelectedNumber_) {
            PlaySE("menuSelect.mp3");
            prevSelectedNumber_ = selectedNumber;
        }

        if (selectedNumber == 1) {
            SetTextureToSprite(spriteMap_["BackToTitle"], "result_0.png");
        }
        else if (selectedNumber == 0) {
            SetTextureToSprite(spriteMap_["BackToTitle"], "result_1.png");
        }

		// 選択肢のスプライトを拡大して表示する
		Vector3 targetScale = GetTextureSizeFromSprite(spriteMap_["BackToTitle"]) * 1.1f;
		SimpleEaseSpriteScale(spriteMap_["BackToTitle"], targetScale, 0.3f);
		// 選択肢のスプライトを上下にゆらゆらさせる
		Vector3 targetTranslate = Vector3(screenCenter_.x, screenCenter_.y + sinf(timer_) * 5.0f, 0.0f);
		SetTranslateToSprite(spriteMap_["BackToTitle"], targetTranslate);
    }

	// 結果の画像が表示されたら選択できるようにする
    if (resultSelector_.IsSelecting() && GetNextSceneName().empty()) {
		int selectedNumber = resultSelector_.GetSelectNumber(); 
        if (selectedNumber == 0) {
            SetNextSceneName("TitleScene");
        }
        else if (selectedNumber == 1) {
            SetNextSceneName("GameScene");
        }

		PlaySE("menuDecide.mp3");

        if (auto* out = GetSceneComponent<SceneChangeOut>()) {
            out->Play();
        }
    }

    // 30秒経過したらタイトルへ自動遷移
    /*if (timer_ >= 30.0f && GetNextSceneName().empty()) {
        SetNextSceneName("TitleScene");
        PlaySE("menuDecide.mp3");
        if (auto* out = GetSceneComponent<SceneChangeOut>()) {
            out->Play();
        }
    }*/

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

    // 結果セレクタをいじれるようにする
    if (!isReadyToSelect_) {
        auto* ic = GetInputCommand();
        if (ic->Evaluate("Submit").Triggered()) {
            isReadyToSelect_ = true;
            PlaySE("menuDecide.mp3");
        }
    }
}

} // namespace KashipanEngine
