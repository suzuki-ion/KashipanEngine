#include "PuzzlePlayer.h"

void Application::PuzzlePlayer::Initialize() {
	maxHp_ = 200;
	hp_ = maxHp_;

	healTimer_ = 0.0f;
	healInterval_ = 1.0f; // 1秒ごとにHPを回復する例
	healAmount_ = 5; // 回復量

	healCooldown_ = 0.0f;
	healCooldownDuration_ = 3.0f; // ダメージを受けてから回復が始まるまでのクールダウン時間

	isAlive_ = true;
}

void Application::PuzzlePlayer::Update(float delta) {
	// プレイヤーが死んでいる場合は回復処理を行わない
	if (!isAlive_) {
		return;
	}

	// ダメージを受けた後の回復開始までのクールダウン処理
	if (healCooldown_ > 0.0f) {
		healCooldown_ -= delta;
		if (healCooldown_ < 0.0f) {
			healCooldown_ = 0.0f;
		}
		return; // クールダウン中は回復しない
	}

	// HPの自動回復処理
	healTimer_ += delta;
	if (healTimer_ >= healInterval_) {
		healTimer_ -= healInterval_;
		SetHp(hp_ + healAmount_);
	}
}
