#pragma once
namespace Application {
	/// パズルの盤面を入れ替えた後のクールダウンを管理するクラス
	class PuzzleSwapCooldown {
	public:
		void Initialize(float cooldownTime);
		void Update(float deltaTime);
		void SetOnCooldown() { remainingTime_ = cooldownTime_; }	

		bool IsOnCooldown() const { return remainingTime_ > 0.0f; }

		void SetCooldownTime(float cooldownTime) { cooldownTime_ = cooldownTime; }
		void SetRemainingTime(float remainingTime) { remainingTime_ = remainingTime; }

		float GetCooldownTime() const { return cooldownTime_; }
		float GetRemainingTime() const {return remainingTime_;}
	private:
		float cooldownTime_;
		float remainingTime_;
	};
}