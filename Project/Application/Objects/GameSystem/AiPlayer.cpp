#include "AiPlayer.h"

void Application::AiPlayer::Initialize(int npc)
{
	npcType_ = npc;
	actionTimer_ = 0.0f;
	// NPCのタイプに応じて行動の間隔を設定
	switch (npcType_) {
	case 0: // タイプ0はゆっくり行動する
		actionInterval_ = 1.5f;
		break;
	case 1: // タイプ1は普通の速度で行動する
		actionInterval_ = 1.0f;
		break;
	case 2: // タイプ2は素早く行動する
		actionInterval_ = 0.5f;
		break;
	default:
		actionInterval_ = 1.0f; // デフォルトは普通の速度
		break;
	}
}

void Application::AiPlayer::Update(float delta)
{
	isMoveDown_ = false;
	isMoveLeft_ = false;
	isMoveRight_ = false;
	isMoveUp_ = false;
	isSend_ = false;

	// 行動のタイマーを更新
	actionTimer_ += delta;
	// 行動のタイマーが行動の間隔を超えたら行動する
	if (actionTimer_ >= actionInterval_) {
		// 移動(4方向)、選択、送る の計6通りのいずれかを選択
		int action = rand() % 6;

		switch (action) {
		case 0:
			isMoveUp_ = true;
			break;
		case 1:
			isMoveDown_ = true;
			break;
		case 2:
			isMoveLeft_ = true;
			break;
		case 3:
			isMoveRight_ = true;
			break;
		case 4:
			isSelecting_ = !isSelecting_;
			break;
		case 5:
			isSend_ = true;
			break;
		}

		// 行動したらタイマーをリセットする
		actionTimer_ = 0.0f;
	}
}