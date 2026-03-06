#include "PuzzleSwapCooldown.h"

void Application::PuzzleSwapCooldown::Initialize(float cooldownTime)
{
	cooldownTime_ = cooldownTime;
	remainingTime_ = 0.0f;
}

void Application::PuzzleSwapCooldown::Update(float deltaTime)
{
	if (remainingTime_ > 0.0f) {
		remainingTime_ -= deltaTime;
		if (remainingTime_ < 0.0f) remainingTime_ = 0.0f;
	}
}