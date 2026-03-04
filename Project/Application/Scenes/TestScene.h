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
    /// ボード座標(row, col)からワールド座標を取得
    Vector3 BoardToWorld(int row, int col) const;
    Vector3 BoardToWorld(float row, float col) const;

    /// 3Dオブジェクトの生成と配置
    void CreateStageObjects();
    /// 指定パネルの色を種類に応じて設定
    void ApplyPanelColor(int row, int col);
    /// 全パネルの色と位置を内部データに合わせてリセット
    void SyncAllPanelVisuals();
    /// カーソルオブジェクトの位置を更新
    void UpdateCursorObject();

    // ================================================================
    // アニメーションフェーズ
    // ================================================================
    enum class Phase {
        Idle,      ///< 入力待ち
        Moving,    ///< パネル移動イージング中
        Clearing,  ///< マッチパネル消去イージング中
        Filling,   ///< 補充パネルスライドイージング中
    };
    Phase phase_ = Phase::Idle;

    /// 個別パネルのイージング情報
    struct PanelAnim {
        int row;           ///< アニメーション対象のパネル論理位置
        int col;
        Vector3 startPos;  ///< 開始ワールド座標
        Vector3 endPos;    ///< 終了ワールド座標
        Vector3 startScale;///< 開始スケール
        Vector3 endScale;  ///< 終了スケール
    };

    float phaseTimer_ = 0.0f;  ///< 現フェーズの経過時間
    float phaseDuration_ = 0.0f; ///< 現フェーズの所要時間
    std::vector<PanelAnim> phaseAnims_; ///< 現フェーズのアニメーション一覧

    /// フェーズ進行を更新
    void UpdatePhase(float deltaTime);

    /// パネル移動アクション開始（Moving フェーズへ）
    void StartMoveAction(int direction);
    /// Moving 完了 → マッチ検出 → Clearing or Idle
    void OnMoveFinished();
    /// Clearing 完了 → Filling フェーズへ
    void OnClearFinished();
    /// Filling 完了 → 連鎖マッチ検出 → Clearing or Idle
    void OnFillFinished();

    /// マッチ検出しClearingフェーズを開始。マッチ無しならfalse
    bool StartClearingPhase();
    /// 消去後の補充アニメーション（Fillingフェーズ）を開始
    void StartFillingPhase();

    // ================================================================

    /// パネルがアニメーション中かどうか
    bool IsAnimating() const { return phase_ != Phase::Idle; }

    SceneDefaultVariables *sceneDefaultVariables_ = nullptr;

    // パズルデータ
    Application::PuzzleBoard board_;
    Application::PuzzleCursor cursor_;
    Application::PuzzleGoal combo_;
    Application::PuzzleGameConfig config_;

    // 3Dオブジェクト
    std::vector<Object3DBase*> groundPanels_;    ///< 地面パネル
    std::vector<Object3DBase*> puzzlePanels_;    ///< パズルパネル
    Object3DBase* cursorObject_ = nullptr;       ///< カーソルオブジェクト

    // 消去 → 補充で使うキャッシュ
    std::vector<Application::PuzzleBoard::MatchLine> pendingMatches_;

    // 設定ファイルパス
    static constexpr const char* kConfigPath = "Assets/Application/PuzzleGameConfig.json";
};

} // namespace KashipanEngine