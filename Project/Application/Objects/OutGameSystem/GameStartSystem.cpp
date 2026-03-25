#include "GameStartSystem.h"

void Application::GameStartSystem::Initialize()
{
	startTimer_ = 0.0f;
	gameStarted_ = false;
	isStartSequencePlayed_ = false;
}

void Application::GameStartSystem::Update(float delta)
{
	if (gameStarted_) return;
	
	if (delta >= 0.016f) delta = 0.016f;

	startTimer_ += delta;
	if (startTimer_ >= startDelay_) {
		gameStarted_ = true;
	}
}

void Application::GameStartSystem::StartSequence(float delay)
{
	if (isStartSequencePlayed_) return;
	isStartSequencePlayed_ = true;

	startDelay_ = delay;
	startTimer_ = 0.0f;
}
