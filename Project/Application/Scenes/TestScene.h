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

namespace KashipanEngine {

class TestScene final : public SceneBase {
public:
    TestScene();
    ~TestScene() override;

    void Initialize() override;

protected:
    void OnUpdate() override;

private:
    // ================================================================
    // 対戦処理
    // ================================================================

    /// 攻撃処理（攻撃側 → 防御側）
    void ProcessAttack(Application::PuzzlePlayer& attacker, Application::PuzzlePlayer& defender);

    /// ロック対象をランダムに選んで適用
    void ApplyLockToDefender(Application::PuzzlePlayer& defender,
        const Application::PuzzlePlayer::MatchSummary& summary,
        float lockTime);

    /// 勝敗判定
    void CheckWinCondition();

    // ================================================================

    SceneDefaultVariables *sceneDefaultVariables_ = nullptr;

    Application::PuzzleGameConfig config_;

    // プレイヤー1（左側）
    Sprite* player1ParentSprite_ = nullptr;
    Transform2D* player1ParentTransform_ = nullptr;
    Application::PuzzlePlayer player1_;

    // プレイヤー2（右側）/ NPC
    Sprite* player2ParentSprite_ = nullptr;
    Transform2D* player2ParentTransform_ = nullptr;
    Application::PuzzlePlayer player2_;

    // NPC（対NPC戦の場合）
    bool isNPCMode_ = true;
    Application::PuzzleNPC npc_;
    Application::PuzzleNPC::Difficulty npcDifficulty_ = Application::PuzzleNPC::Difficulty::Normal;

    // ゲーム状態
    bool gameOver_ = false;
    int winner_ = 0; // 0=未決定, 1=P1勝利, 2=P2勝利

    // 結果テキスト
    Text* resultText_ = nullptr;

    // 背景スプライト
    Sprite* backgroundSprite_ = nullptr;

    // 設定ファイルパス
    static constexpr const char* kConfigPath = "Assets/Application/PuzzleGameConfig.json";
};

} // namespace KashipanEngine