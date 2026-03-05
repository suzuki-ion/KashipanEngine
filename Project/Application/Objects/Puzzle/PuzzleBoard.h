#pragma once
#include <KashipanEngine.h>
#include <vector>
#include <deque>
#include <utility>

namespace Application {

/// パズルボードのデータ管理クラス
/// 二次元配列でパネルの種類を管理する。
/// 正の値: 通常パネル(1～panelTypeCount)
/// kGarbageType(-1): お邪魔パネル
class PuzzleBoard {
public:
	/// お邪魔パネルの種類値
	static constexpr int kGarbageType = -1;

	/// ボードの初期化
	void Initialize(int size, int panelTypeCount);

	int GetSize() const { return size_; }
	int GetPanelTypeCount() const { return panelTypeCount_; }
	int GetPanel(int row, int col) const;
	void SetPanel(int row, int col, int type);

	/// お邪魔パネルかどうか
	bool IsGarbage(int row, int col) const;

	/// お邪魔パネルの数を返す
	int CountGarbage() const;

	/// ランダムな位置にお邪魔パネルを置く（通常パネルを上書き）
	/// @param count 置く数
	/// @return 実際に置いた数
	int PlaceGarbageRandom(int count);

	/// ランダムなお邪魔パネルを1つ除去し通常パネルに置き換える
	/// @return 除去した場合true
	bool RemoveOneGarbageRandom();

	void ShiftRowRight(int row);
	void ShiftRowLeft(int row);
	void ShiftColUp(int col);
	void ShiftColDown(int col);

	// ================================================================
	// マッチパターン
	// ================================================================

	enum class MatchType {
		Normal,
		Straight,
		Cross,
		Square,
	};

	struct MatchResult {
		MatchType type;
		int panelType;
		std::vector<std::pair<int, int>> cells;
		bool isHorizontal = false;
		int fixedIndex = 0;
	};

	/// 全マッチパターンを検出する
	/// お邪魔パネルはマッチ対象外だが、消えるパネルに隣接する場合は一緒に消える
	std::vector<MatchResult> DetectAllMatches(int normalMin, int straightMin) const;

	/// マッチしたパネルを消去し新パネルを補充する
	int ClearAndFillMatches(const std::vector<MatchResult>& matches);

	bool IsEmpty() const;

	const std::vector<std::vector<int>>& GetBoard() const { return board_; }
	void SetBoard(const std::vector<std::vector<int>>& board);

	int GetNextPanelFromTable();

private:
	void RefillSpawnTable();

	struct LineSeg {
		bool isHorizontal;
		int fixedIndex;
		int start;
		int length;
		int type;
	};
	std::vector<LineSeg> DetectLineMatches(int minCount) const;

	int size_ = 0;
	int panelTypeCount_ = 0;
	std::vector<std::vector<int>> board_;
	std::deque<int> spawnTable_;
};

} // namespace Application
