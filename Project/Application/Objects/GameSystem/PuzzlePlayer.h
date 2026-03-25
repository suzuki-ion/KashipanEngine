#pragma once
#include <functional>
#include <algorithm>

namespace Application {
	class PuzzlePlayer {
	public:
		virtual ~PuzzlePlayer() = default;
		void Initialize();
		void Update(float delta);

		void SetMoveUpFunction(std::function<bool()> func) { moveUpFunction_ = func; }
		void SetMoveDownFunction(std::function<bool()> func) { moveDownFunction_ = func; }
		void SetMoveLeftFunction(std::function<bool()> func) { moveLeftFunction_ = func; }
		void SetMoveRightFunction(std::function<bool()> func) { moveRightFunction_ = func; }
		void SetSelectFunction(std::function<bool()> func) { selectFunction_ = func; }
		void SetSendFunction(std::function<bool()> func) { sendFunction_ = func; }

		bool IsMoveUp() const {
			return moveUpFunction_ ? moveUpFunction_() : false;
		}
		bool IsMoveDown() const {
			return moveDownFunction_ ? moveDownFunction_() : false;
		}
		bool IsMoveLeft() const {
			return moveLeftFunction_ ? moveLeftFunction_() : false;
		}
		bool IsMoveRight() const {
			return moveRightFunction_ ? moveRightFunction_() : false;
		}
		bool IsSelect() const {
			return selectFunction_ ? selectFunction_() : false;
		}
		bool IsSend() const {
			return sendFunction_ ? sendFunction_() : false;
		}

		void SetHp(int hp) {
			hp_ = std::clamp(hp, 0, maxHp_);
		}
		int GetHp() const { return hp_; }

		void TakeDamage(int damage) {
			SetHp(hp_ - damage);
			// ダメージを受けたら回復のクールダウンをリセット
			healCooldown_ = healCooldownDuration_;
			// プレイヤーが死んでいるかどうかを更新
			isAlive_ = hp_ > 0;
		}

		void SetMaxHp(int maxHp) {
			maxHp_ = std::max(1, maxHp);
			if (hp_ > maxHp_) {
				hp_ = maxHp_;
			}
		}
		int GetMaxHp() const { return maxHp_; }

		float GetHpRatio() const {
			return maxHp_ > 0 ? static_cast<float>(hp_) / maxHp_ : 0.0f;
		}

		int GetDefaultMaxHp() const { return defaultMaxHp_; }

		float GetHealCooldown() const { return healCooldown_; }
		float GetHealCooldownDuration() const { return healCooldownDuration_; }

		float GetHealTimer() const { return healTimer_; }
		float GetHealInterval() const { return healInterval_; }

		bool IsAlive() const { return isAlive_; }

	private:
		std::function<bool()> moveUpFunction_;
		std::function<bool()> moveDownFunction_;
		std::function<bool()> moveLeftFunction_;
		std::function<bool()> moveRightFunction_;
		std::function<bool()> selectFunction_;
		std::function<bool()> sendFunction_;

		const int defaultMaxHp_ = 200;
		int maxHp_ = 200;
		int hp_ = 200;

		float healCooldown_ = 0.0f;
		float healCooldownDuration_ = 5.0f; // ダメージを受けてから回復が始まるまでのクールダウン時間

		float healTimer_ = 0.0f;
		float healInterval_ = 5.0f; // 5秒ごとにHPを回復する例
		int healAmount_ = 10; // 回復量

		bool isAlive_ = true;
	};
}