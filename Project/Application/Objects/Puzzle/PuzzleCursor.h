#pragma once
#include <KashipanEngine.h>
#include <utility>

namespace Application {

/// パズルゲーム用カーソル
/// カーソルは常にステージ上のどこかしらのパズルパネルを選択している
/// Space/Aボタンを押しながら方向入力で行/列の移動アクションを行う
class PuzzleCursor {
public:
	void Initialize(int startRow, int startCol, int boardSize, float easingDuration);

	/// カーソル入力の更新
	/// @param inputCommand 入力コマンド
	/// @param deltaTime デルタタイム
	/// @param panelsAnimating パズルパネルがアニメーション中かどうか
	void Update(KashipanEngine::InputCommand* inputCommand, float deltaTime, bool panelsAnimating);

	/// 現在の論理位置（行, 列）
	std::pair<int, int> GetPosition() const { return { row_, col_ }; }

	/// 現在の補間済み位置（行, 列）を取得（float）
	std::pair<float, float> GetInterpolatedPosition() const { return { currentRow_, currentCol_ }; }

	/// カーソルがイージング中かどうか
	bool IsMoving() const { return isMoving_; }

	/// 移動アクションが発生したかどうか（1フレームだけtrue）
	bool HasMoveAction() const { return hasMoveAction_; }

	/// 移動アクションの方向を取得（0=上, 1=下, 2=左, 3=右）
	int GetMoveActionDirection() const { return moveActionDir_; }

	void SetBoardSize(int size) { boardSize_ = size; }
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

	// 移動アクション
	bool hasMoveAction_ = false;
	int moveActionDir_ = -1; ///< 0=上, 1=下, 2=左, 3=右
};

} // namespace Application
