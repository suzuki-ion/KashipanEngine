#pragma once
#include <KashipanEngine.h>
#include <utility>

namespace Application {

/// パズルゲーム用カーソル（イージング移動付き）
class PuzzleCursor {
public:
	void Initialize(int startRow, int startCol, int boardSize, float easingDuration);

	/// カーソル入力の更新
	/// イージング中は入力を受け付けない
	void Update(KashipanEngine::InputCommand* inputCommand, float deltaTime);

	/// 現在の論理位置（行, 列）
	std::pair<int, int> GetPosition() const { return { row_, col_ }; }

	/// 現在の補間済み位置（行, 列）を取得（float）
	std::pair<float, float> GetInterpolatedPosition() const { return { currentRow_, currentCol_ }; }

	/// イージング中かどうか
	bool IsMoving() const { return isMoving_; }

	/// ボードサイズの設定
	void SetBoardSize(int size) { boardSize_ = size; }

	/// イージング時間の設定
	void SetEasingDuration(float duration) { easingDuration_ = duration; }

private:
	int row_ = 0;
	int col_ = 0;
	int boardSize_ = 0;
	float easingDuration_ = 0.1f;

	// イージング用
	bool isMoving_ = false;
	float moveTimer_ = 0.0f;
	float startRow_ = 0.0f;
	float startCol_ = 0.0f;
	float targetRow_ = 0.0f;
	float targetCol_ = 0.0f;
	float currentRow_ = 0.0f;
	float currentCol_ = 0.0f;
};

} // namespace Application
