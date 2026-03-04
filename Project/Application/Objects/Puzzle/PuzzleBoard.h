#pragma once
#include <KashipanEngine.h>
#include <vector>
#include <deque>
#include <utility>

namespace Application {

/// パズルボードのデータ管理クラス
/// 二次元配列でパネルの種類を管理する。
class PuzzleBoard {
public:
	/// ボードの初期化
	/// @param size ボードサイズ（n x n）
	/// @param panelTypeCount パネル種類数
	void Initialize(int size, int panelTypeCount);

	/// ボードのサイズ（n x n）
	int GetSize() const { return size_; }

	/// パネル種類数
	int GetPanelTypeCount() const { return panelTypeCount_; }

	/// 指定位置のパネル種類を取得（1～panelTypeCount）
	int GetPanel(int row, int col) const;

	/// 指定位置のパネル種類を設定
	void SetPanel(int row, int col, int type);

	/// 指定行を右に1マス移動する（ループ）
	void ShiftRowRight(int row);
	/// 指定行を左に1マス移動する（ループ）
	void ShiftRowLeft(int row);
	/// 指定列を上に1マス移動する（ループ）
	void ShiftColUp(int col);
	/// 指定列を下に1マス移動する（ループ）
	void ShiftColDown(int col);

	// ================================================================
	// マッチパターン
	// ================================================================

	/// 消し方の種類
	enum class MatchType {
		Normal,    ///< 直線n個以上
		Straight,  ///< 直線m個以上 (m > n)
		Cross,     ///< 縦3+横3の十字
		Square,    ///< 3x3の9マス
	};

	/// マッチ結果
	struct MatchResult {
		MatchType type;
		int panelType;             ///< パネル種類
		std::vector<std::pair<int, int>> cells; ///< 消えるセルの一覧
		bool isHorizontal = false; ///< Normal/Straightの場合の方向
		int fixedIndex = 0;        ///< Normal/Straightの場合の固定軸
	};

	/// 全マッチパターンを検出する（Square > Cross > Straight > Normal の優先順）
	/// @param normalMin ノーマルの最低個数
	/// @param straightMin ストレートの最低個数
	std::vector<MatchResult> DetectAllMatches(int normalMin, int straightMin) const;

	/// マッチしたパネルを消去し、仕様に基づいて新パネルを補充する
	/// @param matches 消去するマッチリスト
	/// @return 実際に消去されたマス数
	int ClearAndFillMatches(const std::vector<MatchResult>& matches);

	/// ボードが空か（全パネル消去＝ブレイク判定用）
	bool IsEmpty() const;

	/// ボード全体の状態を取得
	const std::vector<std::vector<int>>& GetBoard() const { return board_; }

	/// ボードの状態を設定
	void SetBoard(const std::vector<std::vector<int>>& board);

	/// 出現テーブルから次のパネル種類を取得する
	int GetNextPanelFromTable();

private:
	/// 出現テーブルを再生成しシャッフルする
	void RefillSpawnTable();

	/// 直線マッチ（Normal/Straight）を検出
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

	/// パネル出現テーブル（シャッフル済みキュー）
	std::deque<int> spawnTable_;
};

} // namespace Application
