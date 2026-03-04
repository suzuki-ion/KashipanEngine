#pragma once
#include <KashipanEngine.h>
#include <vector>
#include <deque>
#include <utility>

namespace Application {

/// パズルボードのデータ管理クラス
/// 二次元配列でパネルの種類を管理する。
/// 仕様: n×nのボード、パネル種類数=n、行/列単位の移動（ループ）、3つ以上の直線マッチで消去
class PuzzleBoard {
public:
	/// ボードの初期化
	/// @param size ボードサイズ（n x n）
	void Initialize(int size);

	/// ボードのサイズ（n x n）
	int GetSize() const { return size_; }

	/// パネル種類数（= size）
	int GetPanelTypeCount() const { return size_; }

	/// 指定位置のパネル種類を取得（1～size）
	int GetPanel(int row, int col) const;

	/// 指定位置のパネル種類を設定
	void SetPanel(int row, int col, int type);

	/// 指定行を右に1マス移動する（ループ）
	void ShiftRowRight(int row);

	/// 指定行を左に1マス移動する（ループ）
	void ShiftRowLeft(int row);

	/// 指定列を上に1マス移動する（ループ: 行インデックス増加方向=Z+方向）
	void ShiftColUp(int col);

	/// 指定列を下に1マス移動する（ループ: 行インデックス減少方向=Z-方向）
	void ShiftColDown(int col);

	/// マッチ結果を表す構造体
	struct MatchLine {
		bool isHorizontal;     ///< true=横、false=縦
		int fixedIndex;        ///< 横なら行、縦なら列
		int start;             ///< 開始位置（横なら列、縦なら行）
		int length;            ///< 連続数
		int type;              ///< パネル種類
	};

	/// 3つ以上のマッチを検出する
	/// @return マッチした直線のリスト
	std::vector<MatchLine> DetectMatches() const;

	/// マッチしたパネルを消去し、仕様に基づいて新パネルを補充する
	/// - 横消し: 消えた分だけ左のパネルが右に移動し、左端から新パネルが出現
	/// - 縦消し: 消えた分だけ上のパネルが下に移動し、上端から新パネルが出現
	/// @param matches 消去するマッチリスト
	/// @return 実際に消去されたマス数
	int ClearAndFillMatches(const std::vector<MatchLine>& matches);

	/// ボード全体の状態を取得
	const std::vector<std::vector<int>>& GetBoard() const { return board_; }

	/// ボードの状態を設定
	void SetBoard(const std::vector<std::vector<int>>& board);

	/// 出現テーブルから次のパネル種類を取得する
	int GetNextPanelFromTable();

private:
	/// 出現テーブルを再生成しシャッフルする
	void RefillSpawnTable();

	int size_ = 0;
	std::vector<std::vector<int>> board_;

	/// パネル出現テーブル（シャッフル済みキュー）
	std::deque<int> spawnTable_;
};

} // namespace Application
