#include "Scenes/GameScene.h"

#include "Scenes/Components/GameMenuComponents.h"
#include "Scenes/Components/PuzzleBlockFaller.h"
#include "Scenes/Components/PuzzleBlockNextContainer.h"
#include "Scenes/Components/PuzzleBlockPlacePositions.h"
#include "Scenes/Components/PuzzleBoard.h"
#include "Scenes/Components/PuzzlePlayerComponent.h"
#include "Scenes/Components/SceneChangeIn.h"
#include "Scenes/Components/SceneChangeOut.h"

#include <MatsumotoUtility.h>
#include <Objects/SceneValue.h>

using namespace Application;

namespace KashipanEngine {

GameScene::GameScene()
    : SceneBase("GameScene") {}

void GameScene::Initialize() {
    sceneDefaultVariables_ = GetSceneComponent<SceneDefaultVariables>();

    SetNextSceneName("ResultScene");
    isNpcMode_ = Application::Value::isNpcMode;

    //==================================================
    // シーン変数
    //==================================================

    // タイトルシーンから遷移してきた際のモードをここで受け取る想定
    bool isBattleMode = false;
    if (!TryGetSceneVariable("IsBattleMode", isBattleMode)) {
        AddSceneVariable("IsBattleMode", false);
    }

    // パズルの停止状態（ゲームオーバーやゲームクリア、メニュー表示などでパズルの動きを止めるための変数）
    bool isPuzzleStop = true;
    if (!TryGetSceneVariable("IsPuzzleStop", isPuzzleStop)) {
        AddSceneVariable("IsPuzzleStop", true);
    }

    // リザルトシーン用
    AddSceneVariable("IsGameClear", false);
    AddSceneVariable("IsGameOver", false);

    //==================================================
    // スプライト作成関数
    //==================================================

    createSpriteFunction_ =
        [this](const std::string &name, KashipanEngine::DefaultSampler defaultSampler) {
        return Application::MatsumotoUtility::CreateSpriteObject(
            sceneDefaultVariables_->GetScreenBuffer2D(),
            [this](std::unique_ptr<Object2DBase> obj) { return AddObject2D(std::move(obj)); },
            name,
            defaultSampler);
        };

    createSpriteWithTextureFunction_ =
        [this](const std::string &name, const std::string &textureName, KashipanEngine::DefaultSampler defaultSampler) {
        KashipanEngine::Sprite *sprite = createSpriteFunction_(name, defaultSampler);
        Application::MatsumotoUtility::SetTextureToSprite(sprite, textureName);
        Application::MatsumotoUtility::FitSpriteToTexture(sprite);
        return sprite;
        };

    if (auto *screenBuffer2D = sceneDefaultVariables_->GetScreenBuffer2D()) {
        screenCenter_ = Vector2(static_cast<float>(screenBuffer2D->GetWidth()) * 0.5f, static_cast<float>(screenBuffer2D->GetHeight()) * 0.5f);
    }

    //==================================================
    // オブジェクトの作成
    //==================================================

    backgroundSprite_ = createSpriteWithTextureFunction_("Background", "TitleBG.png", KashipanEngine::DefaultSampler::LinearWrap);
    Application::MatsumotoUtility::SetTranslateToSprite(backgroundSprite_, Vector3(screenCenter_.x, screenCenter_.y, 0.0f));

    AddSceneComponent(std::make_unique<PuzzleBoard>());
    if (GetSceneVariableOr<bool>("IsBattleMode", false)) {
        AddSceneComponent(std::make_unique<PuzzleBoard>());
    }

    auto puzzleBoards = GetSceneComponents<PuzzleBoard>();
    for (auto *board : puzzleBoards) {
        if (board) {
            board->SetBoardSize(17, 6);
        }
    }

    if (!puzzleBoards.empty()) {
        if (auto *tr = puzzleBoards[0]->GetBoardRootTransform()) {
            if (GetSceneVariableOr<bool>("IsBattleMode", false) && puzzleBoards.size() >= 2) {
                const float boardSpanX = static_cast<float>(puzzleBoards[0]->GetBoardWidth() - 1) * puzzleBoards[0]->GetStepX();
                const float halfDistance = (boardSpanX * 0.5f) + 96.0f;
                tr->SetTranslate(Vector3(screenCenter_.x - halfDistance, screenCenter_.y, 0.0f));
            } else {
                tr->SetTranslate(Vector3(screenCenter_.x, screenCenter_.y, 0.0f));
            }
        }
    }
    if (puzzleBoards.size() >= 2) {
        if (auto *tr = puzzleBoards[1]->GetBoardRootTransform()) {
            const float boardSpanX = static_cast<float>(puzzleBoards[1]->GetBoardWidth() - 1) * puzzleBoards[1]->GetStepX();
            const float halfDistance = (boardSpanX * 0.5f) + 96.0f;
            tr->SetTranslate(Vector3(screenCenter_.x + halfDistance, screenCenter_.y, 0.0f));
        }
    }

    for (size_t i = 0; i < puzzleBoards.size(); ++i) {
        auto *board = puzzleBoards[i];
        if (!board) continue;

        auto placeComp = std::make_unique<PuzzleBlockPlacePositions>(board);
        auto *placePtr = placeComp.get();
        AddSceneComponent(std::move(placeComp));

        auto nextComp = std::make_unique<PuzzleBlockNextContainer>();
        auto *nextPtr = nextComp.get();
        AddSceneComponent(std::move(nextComp));

        auto fallerComp = std::make_unique<PuzzleBlockFaller>(board, placePtr, nextPtr);
        auto *fallerPtr = fallerComp.get();
        AddSceneComponent(std::move(fallerComp));

        if (i == 0) {
            AddSceneComponent(std::make_unique<Application::ScenePuzzle::PuzzlePlayer>(
                fallerPtr,
                "P1PuzzleBlockMoveHorizontal",
                "P1PuzzleBlockRotation",
                "P1PuzzleBlockPlace"));
        } else {
            AddSceneComponent(std::make_unique<Application::ScenePuzzle::PuzzlePlayer>(
                fallerPtr,
                "P2PuzzleBlockMoveHorizontal",
                "P2PuzzleBlockRotation",
                "P2PuzzleBlockPlace"));
        }
    }

    AddSceneComponent(std::make_unique<GameMenuComponents>(GetInputCommand()));

    isGameOver_ = false;
    autoSceneChangeTimer_ = 3.0f;

    AudioManager::PlayParams params;
    params.sound = AudioManager::GetSoundHandleFromFileName("bgmGame.mp3");
    params.volume = 0.2f;
    params.loop = true;
    audioPlayer_.AddAudio(params);
    audioPlayer_.ChangeAudio(2.0);

    AddSceneComponent(std::make_unique<SceneChangeIn>());
    AddSceneComponent(std::make_unique<SceneChangeOut>());

    if (auto *in = GetSceneComponent<SceneChangeIn>()) {
        in->Play();
    }
}

GameScene::~GameScene() {}

void GameScene::OnUpdate() {
    if (auto *screenBuffer2D = sceneDefaultVariables_->GetScreenBuffer2D()) {
        screenCenter_ = Vector2(static_cast<float>(screenBuffer2D->GetWidth()) * 0.5f, static_cast<float>(screenBuffer2D->GetHeight()) * 0.5f);
    }

    auto puzzleBoards = GetSceneComponents<PuzzleBoard>();
    if (!puzzleBoards.empty()) {
        if (auto *tr = puzzleBoards[0]->GetBoardRootTransform()) {
            if (GetSceneVariableOr<bool>("IsBattleMode", false) && puzzleBoards.size() >= 2) {
                const float boardSpanX = static_cast<float>(puzzleBoards[0]->GetBoardWidth() - 1) * puzzleBoards[0]->GetStepX();
                const float halfDistance = (boardSpanX * 0.5f) + 96.0f;
                tr->SetTranslate(Vector3(screenCenter_.x - halfDistance, screenCenter_.y, 0.0f));
            } else {
                tr->SetTranslate(Vector3(screenCenter_.x, screenCenter_.y, 0.0f));
            }
        }
    }
    if (puzzleBoards.size() >= 2) {
        if (auto *tr = puzzleBoards[1]->GetBoardRootTransform()) {
            const float boardSpanX = static_cast<float>(puzzleBoards[1]->GetBoardWidth() - 1) * puzzleBoards[1]->GetStepX();
            const float halfDistance = (boardSpanX * 0.5f) + 96.0f;
            tr->SetTranslate(Vector3(screenCenter_.x + halfDistance, screenCenter_.y, 0.0f));
        }
    }

    float deltaTime = GetDeltaTime();

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
                ChangeToNextScene();
            }
        }
    }

    Application::MatsumotoUtility::MoveTextureUVToSprite(backgroundSprite_, Vector2(0.0f, GetDeltaTime()));

    auto *menuComp = GetSceneComponent<GameMenuComponents>();
    const bool canLoop = !isGameOver_ && menuComp && menuComp->IsCanLoop();
    AddSceneVariable("IsPuzzleStop", !canLoop);

    if (menuComp && menuComp->IsRequestSceneChange()) {
        SetNextSceneName(menuComp->GetNextSceneName());
        if (auto *out = GetSceneComponent<SceneChangeOut>()) {
            out->Play();
        }
    }

    if (isGameOver_) {
        autoSceneChangeTimer_ -= deltaTime;
        if (autoSceneChangeTimer_ <= 0.0f) {
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
