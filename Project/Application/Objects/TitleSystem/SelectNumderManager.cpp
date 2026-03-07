#include "SelectNumderManager.h"

void Application::SelectNumderManager::Initialize(std::function<bool()> upNumberFunc, std::function<bool()> downNumberFunc, std::function<bool()> submitNumberFunc, std::function<bool()> cancelNumberFunc)
{
	upNumberFunc_ = upNumberFunc;
	downNumberFunc_ = downNumberFunc;
	submitNumberFunc_ = submitNumberFunc;
	cancelNumberFunc_ = cancelNumberFunc;
	selectNumber_ = 0;
	maxNumber_ = 0;
}

void Application::SelectNumderManager::Update()
{
	if (upNumberFunc_ && upNumberFunc_()) {
		selectNumber_ = (selectNumber_ + 1) % (maxNumber_ + 1);
	}
	if (downNumberFunc_ && downNumberFunc_()) {
		selectNumber_ = (selectNumber_ - 1 + (maxNumber_ + 1)) % (maxNumber_ + 1);
	}
}

void Application::SelectNumderManager::Setup(int maxNumber)
{
	maxNumber_ = maxNumber-1;
	selectNumber_ = 0;
}
