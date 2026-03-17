#pragma once
#include <KashipanEngine.h>

#include <vector>
#include <string>

#include <Objects/Puzzle/PuzzleBoard.h>
#include <Objects/Puzzle/PuzzleCursor.h>
#include <Objects/Puzzle/PuzzleGoal.h>
#include <Objects/Puzzle/PuzzlePlayer.h>
#include <Objects/Puzzle/PuzzleNPC.h>
#include <Config/PuzzleGameConfig.h>

#include <Objects/OutGameSystem/GameTutorialManager.h>
#include <Objects/OutGameSystem/GameStartSystem.h>
#include <Objects/Container/MenuSpriteCotainer.h>
#include <Objects/OutGameSystem/MenuActionManager.h>

namespace KashipanEngine {

class GameScene final : public SceneBase {
public:
    GameScene();
    ~GameScene() override;

    void Initialize() override;

protected:
    void OnUpdate() override;

private:
    void ProcessAttack(Application::PuzzlePlayer& attacker, Application::PuzzlePlayer& defender);
    void CheckWinCondition();

    float autoSceneChangeTimer_ = 3.0f;
    SceneDefaultVariables* sceneDefaultVariables_ = nullptr;

    Application::GameStartSystem gameStartSystem_;
    Sprite* gameStartSprite_ = nullptr;
    Sprite* gameStartGoSprite_ = nullptr;

    Application::PuzzleGameConfig config_;

    // 画面の中心
    Vector2 screenCenter_ = Vector2(960.0f, 540.0f);

    Vector2 menuPosition_;
    Application::MenuActionManager menuActionManager_;
    Application::MenuSpriteContainer menuSpriteContainer_;

    Sprite* player1ParentSprite_ = nullptr;
    Transform2D* player1ParentTransform_ = nullptr;
    Application::PuzzlePlayer player1_;
    Application::GameTutorialManager tutorialManager1P_;

    Sprite* player2ParentSprite_ = nullptr;
    Transform2D* player2ParentTransform_ = nullptr;
    Application::PuzzlePlayer player2_;
    Application::GameTutorialManager tutorialManager2P_;

    bool isNPCMode_ = false;
    Application::PuzzleNPC npc_;
    Application::PuzzleNPC::Difficulty npcDifficulty_ = Application::PuzzleNPC::Difficulty::Normal;

    bool gameOver_ = false;
    int winner_ = 0;

    Text* resultText_ = nullptr;
    Sprite* backgroundSprite_ = nullptr;
    AudioPlayer audioPlayer_;

    static constexpr const char* kConfigPath = "Assets/Application/PuzzleGameConfig.json";
};

} // namespace KashipanEngine