#pragma once
#include <KashipanEngine.h>
#include <Objects/Puzzle/PuzzlePlayer.h>
#include <Config/PuzzleGameConfig.h>
#include <vector>
#include <utility>

namespace Application {

/// パズルゲームNPC AI
class PuzzleNPC {
public:
	enum class Difficulty {
		Easy,
		Normal,
		Hard,
	};

	void Initialize(PuzzlePlayer* player, Difficulty difficulty);

	/// 毎フレーム更新（NPCの思考・行動を行う）
	void Update(float deltaTime);

private:
	/// 次の行動を決定する
	void DecideNextAction();

	/// ランダムな方向に移動する
	void DoRandomMove();

	/// ボードを評価してマッチが生まれる移動を探す
	struct ScoredMove {
		int cursorRow;
		int cursorCol;
		int direction; // 0=上,1=下,2=左,3=右
		int score;     // マッチのスコア（高い方が良い）
	};

	/// 全ての可能な移動を評価し、スコア付きリストを返す
	std::vector<ScoredMove> EvaluateAllMoves() const;

	/// 1つの移動をシミュレートしてスコアを返す
	int SimulateAndScore(int row, int col, int direction) const;

	/// 最も良い移動を実行する（Hard用）
	void DoBestMove();

	/// ある程度良い移動を実行する（Normal用）
	void DoDecentMove();

	PuzzlePlayer* player_ = nullptr;
	Difficulty difficulty_ = Difficulty::Normal;

	float thinkTimer_ = 0.0f;       ///< 次に行動するまでの待ち時間
	float thinkInterval_ = 1.0f;    ///< 行動間隔（難易度で変化）
	float skipChance_ = 0.0f;       ///< 攻撃する確率
	float switchChance_ = 0.1f;     ///< ボードをスイッチする確率
	int maxMovesPerTurn_ = 1;       ///< 1ターンの最大移動回数
	int movesThisTurn_ = 0;
};

} // namespace Application
