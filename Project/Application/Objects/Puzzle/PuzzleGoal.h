#pragma once
#include <KashipanEngine.h>
#include <vector>

namespace Application {

class PuzzleBoard;

/// コンボ管理クラス
/// 1回の移動で複数の色が揃うとその揃った分がコンボとなる
class PuzzleGoal {
public:
	/// 初期化
	void Initialize();

	/// コンボ数を設定
	void SetCombo(int combo) { currentCombo_ = combo; }

	/// 現在のコンボ数を取得
	int GetCurrentCombo() const { return currentCombo_; }

	/// 総コンボ数を取得
	int GetTotalCombo() const { return totalCombo_; }

	/// コンボをリセット
	void ResetCombo() { currentCombo_ = 0; }

	/// コンボを加算
	void AddCombo(int count);

private:
	int currentCombo_ = 0;
	int totalCombo_ = 0;
};

} // namespace Application
