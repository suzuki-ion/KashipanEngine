#pragma once
namespace Application {
	// ゲーム開始前の処理を行うシステム
	class GameStartSystem {
	public:
		void Initialize();
		void Update(float delta);

		void StartSequence(float delay);

		void SetStartDelay(float delay) { startDelay_ = delay; }
		bool IsGameStarted() const { return gameStarted_; }

	private:
		float startTimer_;
		float startDelay_;
		bool gameStarted_ = false;
		bool isStartSequencePlayed_ = false;

	};
}
