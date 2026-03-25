#include "ResultSelector.h"

void Application::ResultSelector::Initialize(std::function<bool()> upFunc, std::function<bool()> downFunc, std::function<bool()> submitFunc, std::function<bool()> cancelFunc)
{
	upFunc_ = upFunc;
	downFunc_ = downFunc;
	submitFunc_ = submitFunc;
	cancelFunc_ = cancelFunc;
	selecting_ = false;

	selectNumderManager_.Initialize(upFunc, downFunc, submitFunc, cancelFunc);
	selectNumderManager_.Setup(2);
}

void Application::ResultSelector::Update()
{
	if (selecting_) {
		return;
	}

	selectNumderManager_.Update();
	if (submitFunc_()) {
		selecting_ = true;
	}
}
