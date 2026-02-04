#pragma once
#include "Scenes/Components/Tutorial/TutorialBase.h"
#include <functional>
#include <array>

namespace KashipanEngine {

class PlayerMove;
class BombManager;
class BPMSystem;
class ExplosionManager;

/// @brief チュートリアルの種類
enum class TutorialType {
	UseTutorial,           // チュートリアルを使用するか確認
    Movement,              // 移動チュートリアル
    BombPlaceAndDetonate,  // 爆弾設置→起爆チュートリアル
    BeatBombPlace,         // 拍に合わせて爆弾設置チュートリアル
    ExplosionMove,         // 爆発に巻き込まれて移動チュートリアル
	MissonText,            // ミッションテキスト表示、押したらMenuに行く
    Count                  // チュートリアルの総数
};

/// @brief UseTutorial選択肢
enum class UseTutorialSelection {
    Yes,
    No
};

/// @brief チュートリアルの進行状態
enum class TutorialPhase {
    Initial,          // 初期状態（説明表示前）
    ShowInstruction,  // モニターに説明を表示中
    WaitForConfirm,   // 確認ボタン待ち
    Practicing,       // 実践中
    Completed,        // 完了（次のチュートリアルへ or 終了）
    Finished          // 全チュートリアル完了
};

/// @brief 複数のチュートリアルを管理するマネージャークラス
class TutorialManager : public TutorialBase {
public:
    TutorialManager(InputCommand* inputCommand,
        const Vector3& gameTargetPos,
        const Vector3& gameTargetRot,
        const Vector3& menuTargetPos,
        const Vector3& menuTargetRot);

    ~TutorialManager() override = default;

    void Initialize() override;
    void Update() override;

    /// @brief プレイヤーのPlayerMoveコンポーネントを設定
    void SetPlayerMove(PlayerMove* playerMove) { playerMove_ = playerMove; }

    /// @brief BombManagerを設定
    void SetBombManager(BombManager* bombManager) { bombManager_ = bombManager; }

    /// @brief BPMSystemを設定
    void SetBPMSystem(BPMSystem* bpmSystem) { bpmSystem_ = bpmSystem; }

    /// @brief ExplosionManagerを設定
    void SetExplosionManager(ExplosionManager* explosionManager) { explosionManager_ = explosionManager; }

    /// @brief プレイヤーオブジェクトを設定
    void SetPlayer(Object3DBase* player) { player_ = player; }

    /// @brief モニターのScreenBufferを設定
    void SetMonitorScreenBuffer(ScreenBuffer* screenBuffer) { monitorScreenBuffer_ = screenBuffer; }

    /// @brief 全チュートリアル完了時のコールバックを設定
    void SetOnAllTutorialsCompletedCallback(std::function<void()> callback) {
        onAllTutorialsCompleted_ = std::move(callback);
    }

    /// @brief チュートリアルを開始
    void StartTutorials();

    /// @brief チュートリアルをスキップ（デバッグ用）
    void SkipAllTutorials();

    /// @brief 現在のチュートリアルタイプを取得
    TutorialType GetCurrentTutorialType() const { return currentTutorial_; }

    /// @brief 現在のフェーズを取得
    TutorialPhase GetCurrentPhase() const { return currentPhase_; }

    /// @brief 全チュートリアルが完了したかどうか
    bool IsAllTutorialsCompleted() const { return currentPhase_ == TutorialPhase::Finished; }

    /// @brief 移動回数をインクリメント（PlayerMoveから呼び出し用）
    void IncrementMoveCount() { moveCount_++; }

    /// @brief 爆弾設置回数をインクリメント（BombManagerから呼び出し用）
    void IncrementBombCount() { bombCount_++; }

    /// @brief 爆発回数をインクリメント（ExplosionManagerから呼び出し用）
    void IncrementExplosionCount() { explosionCount_++; }

    /// @brief 拍に合わせた爆弾設置回数をインクリメント
    void IncrementBeatBombCount() { beatBombCount_++; }

    /// @brief 爆発による移動回数をインクリメント
    void IncrementExplosionMoveCount() { explosionMoveCount_++; }

    /// @brief BombManagerとPlayerMoveが入力を受け付けられる状態かを取得
    bool CanAcceptInput() const { 
        // Practicingフェーズ中かつ入力ブロック中でない場合のみ入力を受け付ける
        return currentPhase_ == TutorialPhase::Practicing && !isInputBlocked_; 
    }

    /// @brief チュートリアルモデルを初期化
    void InitializeTutorialModels();

private:
    /// @brief 次のチュートリアルへ進む
    void AdvanceToNextTutorial();

    /// @brief チュートリアル完了後のリセット処理
    void ResetTutorialEnvironment();

    /// @brief 現在のチュートリアルの実践フェーズを更新
    void UpdatePracticing();

    /// @brief 移動チュートリアルの実践更新
    void UpdateMovementPractice();

    /// @brief 爆弾設置→起爆チュートリアルの実践更新
    void UpdateBombPlaceAndDetonatePractice();

    /// @brief 拍に合わせて爆弾設置チュートリアルの実践更新
    void UpdateBeatBombPlacePractice();

    /// @brief 爆発移動チュートリアルの実践更新
    void UpdateExplosionMovePractice();

    /// @brief ミッションテキストチュートリアルの実践更新
    void UpdateMissonTextPractice();

    /// @brief 現在のチュートリアルが完了条件を満たしたかチェック
    bool IsCurrentTutorialComplete() const;

    /// @brief 現在のチュートリアルの必要回数を取得
    int GetRequiredCount() const;

    /// @brief 現在のチュートリアルの達成回数を取得
    int GetCurrentCount() const;

    /// @brief チュートリアルモデルの表示を更新
    void UpdateTutorialModelDisplay();

    /// @brief すべてのチュートリアルモデルを非表示にする
    void HideAllTutorialModels();

    /// @brief UseTutorialの選択を更新（左右入力）
    void UpdateUseTutorialSelection();

    /// @brief UseTutorialの表示を更新
    void UpdateUseTutorialDisplay();

    /// @brief Yes/Noオプションを非表示にする
    void HideUseTutorialOptions();

private:
    PlayerMove* playerMove_ = nullptr;
    BombManager* bombManager_ = nullptr;
    BPMSystem* bpmSystem_ = nullptr;
    ExplosionManager* explosionManager_ = nullptr;
    Object3DBase* player_ = nullptr;
    ScreenBuffer* monitorScreenBuffer_ = nullptr;

    TutorialType currentTutorial_ = TutorialType::UseTutorial;
    TutorialPhase currentPhase_ = TutorialPhase::Initial;

    std::function<void()> onAllTutorialsCompleted_;

    // チュートリアルモデル（turtrial_1.obj ～ turtrial_4.obj）
    static constexpr int kTutorialModelCount = 6;
    std::array<Object3DBase*, kTutorialModelCount> tutorialModels_{};
	Object3DBase* useTutorialYes_ = nullptr;
    Object3DBase* useTutorialNo_ = nullptr;
    bool tutorialModelsInitialized_ = false;

    // 移動チュートリアル用
    int moveCount_ = 0;
    int requiredMoves_ = 4;  // 必要な移動回数
    Vector3 lastPlayerPosition_{};
    bool isTrackingMovement_ = false;

    // 爆弾設置→起爆チュートリアル用
    int bombCount_ = 0;
    int explosionCount_ = 0;
    int requiredBombDetonations_ = 2;  // 必要な爆弾設置→起爆回数
    int lastActiveBombCount_ = 0;
    bool bombPlaced_ = false;  // 爆弾が設置されたかどうか

    // 拍に合わせて爆弾設置チュートリアル用
    int beatBombCount_ = 0;
    int requiredBeatBombs_ = 1;  // 必要な拍合わせ爆弾設置回数

    // 爆発移動チュートリアル用
    int explosionMoveCount_ = 0;
    int requiredExplosionMoves_ = 2;  // 必要な爆発移動回数

    // 拍に合わせた爆弾設置の監視用
    int lastBeatBombCount_ = 0;
    bool wasInBPMRange_ = false;

    // 爆発移動の監視用
    bool wasKnockedBack_ = false;

    // 移動チュートリアルの監視用
    bool wasMoving_ = false;

    // 待機タイマー（チュートリアル間の遷移用）
    float waitTimer_ = 0.0f;
    float waitDuration_ = 0.5f;
    float completedWaitDuration_ = 0.5f;  // ミッション完了後の待機時間
    bool isWaiting_ = false;

    // 入力ブロック（確認ボタン押下後の誤入力防止）
    bool isInputBlocked_ = false;
    float inputBlockTimer_ = 0.0f;
    float inputBlockDuration_ = 0.3f;  // 入力を無効にする時間

    // UseTutorial選択状態
    UseTutorialSelection currentSelection_ = UseTutorialSelection::Yes;
    bool useTutorialDecided_ = false;  // UseTutorial選択が完了したか

    GameTimer moveTimer_;
    GameTimer bombTimer_;
};

} // namespace KashipanEngine
