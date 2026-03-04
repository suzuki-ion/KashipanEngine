#pragma once
#include <KashipanEngine.h>
#include <vector>
#include <string>
#include <map>
#include <functional>

#include <Objects/Puzzle/PuzzleBoard.h>
#include <Objects/Puzzle/PuzzleCursor.h>
#include <Objects/Puzzle/PuzzleGoal.h>
#include <Config/PuzzleGameConfig.h>

namespace Application {

/// 1プレイヤー分のパズルステージ・UI・状態を管理するクラス
class PuzzlePlayer {
public:
	/// アニメーションフェーズ
	enum class Phase {
		Idle,
		Moving,
		Clearing,
		Filling,
	};

	/// マッチ集計結果
	struct MatchSummary {
		int normalCount = 0;
		int straightCount = 0;
		int crossCount = 0;
		int squareCount = 0;
		int comboCount = 0;
		bool isBreak = false;
	};

	/// ロック情報
	struct LockInfo {
		float remainingTime = 0.0f;
	};

	/// オブジェクト追加用コールバック型
	using AddObject2DFunc = std::function<bool(std::unique_ptr<KashipanEngine::Object2DBase>)>;

	void Initialize(
		const PuzzleGameConfig& config,
		KashipanEngine::ScreenBuffer* screenBuffer2D,
		KashipanEngine::Window* window,
		AddObject2DFunc addObject2DFunc,
		KashipanEngine::Transform2D* parentTransform,
		const std::string& playerName,
		const std::string& commandPrefix = "Puzzle");

	/// 毎フレーム更新
	void Update(float deltaTime, KashipanEngine::InputCommand* inputCommand);

	// ================================================================
	// 外部操作（NPC/対戦処理から呼ぶ）
	// ================================================================

	/// HPを減らす
	void ApplyDamage(int damage);
	/// 行/列ロックを追加
	void ApplyLock(bool isRow, int index, float seconds);
	/// 既存ロック全てに秒数を加算
	void AddToExistingLocks(float seconds);

	// ================================================================
	// 状態取得
	// ================================================================

	int GetHP() const { return hp_; }
	bool IsDead() const { return hp_ <= 0; }
	bool IsAnimating() const { return phase_ != Phase::Idle; }
	Phase GetPhase() const { return phase_; }
	float GetTimer() const { return timer_; }
	float GetTimeLimit() const { return config_.timeLimit; }
	const MatchSummary& GetLastMatchSummary() const { return lastMatchSummary_; }
	bool HasPendingAttack() const { return hasPendingAttack_; }
	void ClearPendingAttack() { hasPendingAttack_ = false; }
	float GetRemainingTimeAtSkip() const { return remainingTimeAtSkip_; }
	const PuzzleBoard& GetBoard() const { return board_; }
	const PuzzleGoal& GetCombo() const { return combo_; }
	int GetBoardSize() const { return config_.stageSize; }
	const PuzzleGameConfig& GetConfig() const { return config_; }

	/// 行のロック情報
	const std::map<int, LockInfo>& GetRowLocks() const { return rowLocks_; }
	/// 列のロック情報
	const std::map<int, LockInfo>& GetColLocks() const { return colLocks_; }

	/// NPC用：カーソル位置を強制設定
	void SetCursorPosition(int row, int col);
	/// NPC用：移動アクションを強制実行
	void ForceMove(int direction);
	/// NPC用：時間スキップを強制実行
	void ForceTimeSkip();

	/// 行がロックされているか
	bool IsRowLocked(int row) const;
	/// 列がロックされているか
	bool IsColLocked(int col) const;

	/// カーソル位置取得
	std::pair<int, int> GetCursorPosition() const { return cursor_.GetPosition(); }

private:
	// ================================================================
	// 座標変換
	// ================================================================
	Vector2 BoardToScreen(int row, int col) const;
	Vector2 BoardToScreen(float row, float col) const;

	// ================================================================
	// スプライト生成
	// ================================================================
	void CreateSprites();
	void ApplyPanelColor(int row, int col);
	void SyncAllPanelVisuals();
	void UpdateCursorSprite();
	void UpdateLockOverlays();
	void UpdateTimerGauge();
	void UpdateHPGauge();
	void UpdateMatchText();

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
	// 制限時間
	// ================================================================
	void OnTimerExpired();
	void OnTimeSkip();

	// ================================================================
	// ロック更新
	// ================================================================
	void UpdateLocks(float deltaTime);

	// ================================================================
	// ダメージ/ロック計算
	// ================================================================
	void CalculateAttackFromMatches();

	// ================================================================
	// データ
	// ================================================================
	PuzzleGameConfig config_;
	PuzzleBoard board_;
	PuzzleCursor cursor_;
	PuzzleGoal combo_;

	int hp_ = 100;
	float timer_ = 0.0f;
	bool timerActive_ = false;
	Phase phase_ = Phase::Idle;

	float phaseTimer_ = 0.0f;
	float phaseDuration_ = 0.0f;
	std::vector<PanelAnim> phaseAnims_;
	std::vector<PuzzleBoard::MatchResult> pendingMatches_;

	// 攻撃結果
	MatchSummary lastMatchSummary_{};
	bool hasPendingAttack_ = false;
	int pendingDamage_ = 0;
	float pendingLockTime_ = 0.0f;
	float remainingTimeAtSkip_ = 0.0f;

	// ロック
	std::map<int, LockInfo> rowLocks_;
	std::map<int, LockInfo> colLocks_;

	// ================================================================
	// スプライト
	// ================================================================
	KashipanEngine::ScreenBuffer* screenBuffer2D_ = nullptr;
	KashipanEngine::Window* window_ = nullptr;
	AddObject2DFunc addObject2DFunc_;
	KashipanEngine::Transform2D* parentTransform_ = nullptr;
	std::string playerName_;

	std::vector<KashipanEngine::Sprite*> stagePanelSprites_;
	std::vector<KashipanEngine::Sprite*> puzzlePanelSprites_;
	KashipanEngine::Sprite* cursorSprite_ = nullptr;

	// ロックオーバーレイスプライト（行/列）
	std::vector<KashipanEngine::Sprite*> rowLockSprites_;
	std::vector<KashipanEngine::Sprite*> colLockSprites_;

	// タイマーゲージ
	KashipanEngine::Sprite* timerGaugeBgSprite_ = nullptr;
	KashipanEngine::Sprite* timerGaugeFillSprite_ = nullptr;

	// HPゲージ
	KashipanEngine::Sprite* hpGaugeBgSprite_ = nullptr;
	KashipanEngine::Sprite* hpGaugeFillSprite_ = nullptr;

	// マッチ/コンボテキスト
	KashipanEngine::Text* matchText_ = nullptr;
	KashipanEngine::Text* comboText_ = nullptr;
	float matchTextTimer_ = 0.0f;
	static constexpr float kMatchTextDuration = 2.0f;

	// コマンドプレフィックス
	std::string commandPrefix_ = "Puzzle";
	std::string cmdTimeSkip_ = "PuzzleTimeSkip";
};

} // namespace Application
