#pragma once
#include <KashipanEngine.h>

#include <vector>

#include <Objects/Puzzle/PuzzleBoard.h>
#include <Objects/Puzzle/PuzzleCursor.h>
#include <Objects/Puzzle/PuzzleGoal.h>
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
    SceneDefaultVariables *sceneDefaultVariables_ = nullptr;

    // 設定
    Application::PuzzleGameConfig config_;
    static constexpr const char* kConfigPath = "Assets/Application/PuzzleGameConfig.json";

    // ゲームロジック
    Application::PuzzleBoard puzzleBoard_;
    Application::PuzzleCursor puzzleCursor_;
    Application::PuzzleGoal puzzleGoal_;

    // 3Dオブジェクト：地面パネル [row][col]
    std::vector<std::vector<Box*>> groundPanels_;
    // 3Dオブジェクト：パズルパネル [row][col] （空きマスはnullptr）
    std::vector<std::vector<Box*>> puzzlePanels_;
    // カーソル用3Dオブジェクト
    Box* cursorBox_ = nullptr;
    // 目標パネル地面 [3][3]
    std::vector<std::vector<Box*>> goalGroundPanels_;
    // 目標パネル [3][3]
    std::vector<std::vector<Box*>> goalPanels_;

    // ステージ上部の遮光オブジェクト（中央3x3以外を覆う）
    std::vector<Box*> roofPanels_;

    // パネルイージング管理
    struct PanelAnimation {
        int fromRow, fromCol;
        int toRow, toCol;
        float timer = 0.0f;
        bool active = false;
    };
    PanelAnimation panelAnim_;

    // ヘルパー関数
    /// グリッド座標からワールド座標を計算
    Vector3 GridToWorld(int row, int col) const;
    /// 目標パネルのグリッド座標からワールド座標を計算
    Vector3 GoalGridToWorld(int row, int col) const;
    /// パズルパネルの色を更新
    void UpdatePanelColors();
    /// 目標パネルの色を更新
    void UpdateGoalPanelColors();
    /// カメラを設定
    void SetupCamera();
};

} // namespace KashipanEngine