#pragma once
#include <KashipanEngine.h>
#include <vector>
#include <string>
#include <map>
#include <functional>

#include <Objects/Puzzle/PuzzleBoard.h>
#include <Objects/Puzzle/PuzzleCursor.h>
#include <Objects/Puzzle/PuzzleGoal.h>
#include <Objects/Puzzle/PuzzleSwapCooldown.h>
#include <Config/PuzzleGameConfig.h>

namespace Application {

/// 1プレイヤー分のパズルステージ・UI・状態を管理するクラス
/// 2つのボード（A/B）を持ち、切り替えて使用する
class PuzzlePlayer {
public:
	enum class Phase {
		Idle,
		Moving,
		Clearing,
		Filling,
	};

	struct MatchSummary {
		int normalCount = 0;
		int straightCount = 0;
		int crossCount = 0;
		int squareCount = 0;
		int comboCount = 0;
		bool isBreak = false;
		int totalClearedCells = 0;
		int garbageClearedCells = 0;
	};

	struct LockInfo {
		float remainingTime = 0.0f;
	};

	/// 相手から送られてくるお邪魔パネルの遅延キュー要素
	struct GarbageQueueEntry {
		float remainingTime = 0.0f;
		float totalTime = 0.0f;
		float garbageAmount = 0.0f;
	};

	using AddObject2DFunc = std::function<bool(std::unique_ptr<KashipanEngine::Object2DBase>)>;

	void Initialize(
		const PuzzleGameConfig& config,
		KashipanEngine::ScreenBuffer* screenBuffer2D,
		KashipanEngine::Window* window,
		AddObject2DFunc addObject2DFunc,
		KashipanEngine::Transform2D* parentTransform,
		const std::string& playerName,
		const std::string& commandPrefix = "Puzzle",
		bool isPlayer2 = false);

	void Update(float deltaTime, KashipanEngine::InputCommand* inputCommand);

	// ================================================================
	// 外部操作
	// ================================================================

	/// お邪魔パネルの遅延キューを追加する（相手からの攻撃）
	void EnqueueGarbage(float amount, float delayTime);
	/// 行/列ロックを追加（アクティブボード）
	void ApplyLock(bool isRow, int index, float seconds);
	/// 既存ロック全てに秒数を加算
	void AddToExistingLocks(float seconds);
	/// 全ロックを解除
	void ClearAllLocks();

	// ================================================================
	// 状態取得
	// ================================================================

	bool IsDefeated() const;
	bool IsAnimating() const { return phase_ != Phase::Idle; }
	Phase GetPhase() const { return phase_; }
	float GetGameElapsedTime() const { return gameElapsedTime_; }
	const MatchSummary& GetLastMatchSummary() const { return lastMatchSummary_; }
	bool HasPendingAttack() const { return hasPendingAttack_; }
	void ClearPendingAttack() { hasPendingAttack_ = false; }
	int GetPendingGarbageToSend() const { return pendingGarbageToSend_; }
	float GetPendingGarbageToSendFloat() const { return pendingGarbageAccumulator_; }
	void ClearPendingGarbage() { pendingGarbageToSend_ = 0; }
	const PuzzleBoard& GetBoard() const { return GetActiveBoard(); }
	const PuzzleGoal& GetCombo() const { return combo_; }
	int GetBoardSize() const { return config_.stageSize; }
	const PuzzleGameConfig& GetConfig() const { return config_; }
	bool IsMoveCursorMode() const { return cursor_.IsHoldingAction(); }

	/// アクティブボードの崩壊度(0.0~1.0)
	float GetActiveCollapseRatio() const;
	/// 非アクティブボードの崩壊度(0.0~1.0)
	float GetInactiveCollapseRatio() const;
	/// アクティブボードのインデックス
	int GetActiveIndex() const { return activeBoard_; }

	const std::map<int, LockInfo>& GetRowLocks() const { return rowLocks_[activeBoard_]; }
	const std::map<int, LockInfo>& GetColLocks() const { return colLocks_[activeBoard_]; }

	const std::vector<GarbageQueueEntry>& GetGarbageQueue() const { return garbageQueue_; }

	void SetCursorPosition(int row, int col);
	void ForceMove(int direction);
	void ForceAttack();
	void ForceSwitchBoard();

	bool IsRowLocked(int row) const;
	bool IsColLocked(int col) const;
	int GetTotalLockCount() const { return static_cast<int>(rowLocks_[activeBoard_].size() + colLocks_[activeBoard_].size()); }

	std::pair<int, int> GetCursorPosition() const { return cursor_.GetPosition(); }

	// ================================================================
	// ボードトランスフォーム操作
	// ================================================================
	KashipanEngine::Transform2D* GetActiveBoardTransform() { return activeBoardTransform_; }
	KashipanEngine::Transform2D* GetInactiveBoardTransform() { return inactiveBoardTransform_; }
	/// お邪魔パネルキューを相殺する（自分が攻撃した時に呼ばれる）
	/// @return 相殺しきれなかった余剰分
	float OffsetGarbageQueue(float amount);

private:
	// ================================================================
	// ボードアクセス
	// ================================================================
	PuzzleBoard& GetActiveBoard() { return boards_[activeBoard_]; }
	const PuzzleBoard& GetActiveBoard() const { return boards_[activeBoard_]; }
	PuzzleBoard& GetInactiveBoard() { return boards_[1 - activeBoard_]; }
	const PuzzleBoard& GetInactiveBoard() const { return boards_[1 - activeBoard_]; }

	// ================================================================
	// 座標変換
	// ================================================================
	Vector2 BoardToScreen(int row, int col) const;
	Vector2 BoardToScreen(float row, float col) const;

	// ================================================================
	// スプライト生成・更新
	// ================================================================
	void CreateSprites();
	void CreateBoardRootTransforms();
	void ApplyPanelColor(int row, int col);
	void SyncAllPanelVisuals();
	void StartSwapPanelAnimation();
	void UpdateCursorSprite();
	void UpdateLockOverlays();
	void UpdateGarbageQueueGauges();
	void UpdateCollapseGauge();
	void UpdateMatchText();
	void UpdateInactivePreview(float deltaTime);
	void UpdateInactiveLockOverlays();
	void UpdateGarbageWarnings();
	void UpdateMoveGarbageWarnings();
	void UpdateSwapPanelAnimations(float deltaTime);
	void UpdateSwapCoolDownSpriteAnimation(float deltaTime);

	// ================================================================
	// 移動時お邪魔パネル予告位置計算
	// ================================================================
	void PreCalculateMoveGarbagePositions();

	// ================================================================
	// シェイク
	// ================================================================
	void UpdateShake(float deltaTime);

	// ================================================================
	// アニメーションフェーズ
	// ================================================================
	struct PanelAnim {
		int row;
		int col;
		Vector2 startPos;
		Vector2 endPos;
		Vector2 startScale;
		Vector2 endScale;
	};

	void UpdatePhase(float deltaTime);
	void StartMoveAction(int direction);
	void OnMoveFinished();
	bool StartClearingPhase();
	void OnClearFinished();
	void StartFillingPhase();
	void OnFillFinished();

	// ================================================================
	// 攻撃アクション
	// ================================================================
	void OnAttack();

	// ================================================================
	// ステージ切り替え
	// ================================================================
	void SwitchBoard();
	/// 自動ステージ切り替え（崩壊度が閾値に達した場合）
	void AutoSwitchBoardIfNeeded();

	// ================================================================
	// ロック更新
	// ================================================================
	void UpdateLocks(float deltaTime);

	// ================================================================
	// 攻撃計算
	// ================================================================
	void CalculateAttackFromMatches();

	// ================================================================
	// お邪魔パネルキュー更新
	// ================================================================
	void UpdateGarbageQueue(float deltaTime);

	// ================================================================
	// 非アクティブボード更新
	// ================================================================
	void UpdateInactiveBoard(float deltaTime);

	// ================================================================
	// 時間経過倍率
	// ================================================================
	float GetEscalationMultiplier() const;

	// ================================================================
	// データ
	// ================================================================
	PuzzleGameConfig config_;
	PuzzleBoard boards_[2];
	int activeBoard_ = 0;
	PuzzleCursor cursor_;
	PuzzleGoal combo_;

	float gameElapsedTime_ = 0.0f;
	Phase phase_ = Phase::Idle;

	float phaseTimer_ = 0.0f;
	float phaseDuration_ = 0.0f;
	std::vector<PanelAnim> phaseAnims_;
	std::vector<PuzzleBoard::MatchResult> pendingMatches_;

	MatchSummary lastMatchSummary_{};
	bool hasPendingAttack_ = false;
	int pendingGarbageToSend_ = 0;
	float pendingGarbageAccumulator_ = 0.0f;
	float pendingLockTime_ = 0.0f;

	// 移動回数カウント（お邪魔パネル出現用）
	int moveCount_ = 0;

	// ロック（ボード別）
	std::map<int, LockInfo> rowLocks_[2];
	std::map<int, LockInfo> colLocks_[2];
	static constexpr int kMaxTotalLocks = 2;

	// 非アクティブボードのお邪魔パネル減少タイマー
	float inactiveDecayTimer_ = 0.0f;

	// シェイク
	float shakeTimer_ = 0.0f;
	float shakeDuration_ = 0.3f;
	float shakeIntensity_ = 10.0f;
	Vector3 parentOriginalPos_{};

	// お邪魔パネル予告位置
	std::vector<std::pair<int, int>> pendingGarbagePositions_;

	// 移動時お邪魔パネルの次回出現予告位置
	std::vector<std::pair<int, int>> nextMoveGarbagePositions_;

	// 入れ替え用のクールダウン管理クラス
	PuzzleSwapCooldown swapCooldown_;
	// お邪魔パネル遅延キュー（相手からの攻撃）
	std::vector<GarbageQueueEntry> garbageQueue_;

	// ================================================================
	// スプライト
	// ================================================================
	KashipanEngine::ScreenBuffer* screenBuffer2D_ = nullptr;
	KashipanEngine::Window* window_ = nullptr;
	AddObject2DFunc addObject2DFunc_;
	KashipanEngine::Transform2D* parentTransform_ = nullptr;
	std::string playerName_;
	bool isPlayer2_ = false;

	// ボードルートトランスフォーム
	KashipanEngine::Transform2D* activeBoardTransform_ = nullptr;
	KashipanEngine::Transform2D* inactiveBoardTransform_ = nullptr;

	std::vector<KashipanEngine::Sprite*> stagePanelSprites_;
	std::vector<KashipanEngine::Sprite*> puzzlePanelSprites_;
	KashipanEngine::Sprite* cursorSprite_ = nullptr;

	std::vector<KashipanEngine::Sprite*> rowLockSprites_;
	std::vector<KashipanEngine::Sprite*> colLockSprites_;

	// お邪魔パネル予告オーバーレイ
	std::vector<KashipanEngine::Sprite*> garbageWarningSprites_;

	// 移動時お邪魔パネル予告オーバーレイ
	std::vector<KashipanEngine::Sprite*> moveGarbageWarningSprites_;

	// お邪魔パネルキューゲージ（背景・Fill・テキスト）
	static constexpr int kMaxGarbageGauges = 8;
	struct GarbageGaugeSprites {
		KashipanEngine::Sprite* bg = nullptr;
		KashipanEngine::Sprite* fill = nullptr;
		KashipanEngine::Text* amountText = nullptr;
	};
	GarbageGaugeSprites garbageGaugeSprites_[kMaxGarbageGauges];

	// 崩壊度テキスト（アクティブ/非アクティブ）
	KashipanEngine::Text* activeCollapseText_ = nullptr;
	KashipanEngine::Text* inactiveCollapseText_ = nullptr;

	// 非アクティブボード小表示スプライト
	std::vector<KashipanEngine::Sprite*> inactivePreviewSprites_;
	KashipanEngine::Sprite* inactivePreviewBg_ = nullptr;

	// 非アクティブボードプレビュー用ロックオーバーレイ
	std::vector<KashipanEngine::Sprite*> inactiveRowLockSprites_;
	std::vector<KashipanEngine::Sprite*> inactiveColLockSprites_;

	KashipanEngine::Text* matchText_ = nullptr;
	KashipanEngine::Text* comboText_ = nullptr;
	float matchTextTimer_ = 0.0f;
	static constexpr float kMatchTextDuration = 2.0f;

	std::string commandPrefix_ = "Puzzle";
	std::string cmdAttack_ = "PuzzleTimeSkip";
	std::string cmdSwitchBoard_ = "PuzzleSwitchBoard";

	// ステージ切り替えのクールダウン用スプライト
	KashipanEngine::Sprite* switchCooldownSprite_;
	KashipanEngine::Sprite* switchCooldownBackGroundSprite_;
};

} // namespace Application
