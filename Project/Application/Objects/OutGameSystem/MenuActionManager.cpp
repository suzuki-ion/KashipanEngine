#include "MenuActionManager.h"
using namespace Application;

void Application::MenuActionManager::Initialize(
	std::function<bool()> menuConditions,
	std::function<bool()> menuSelectAction,
	std::function<bool()> menuCancelAction,
	std::function<bool()> menuIndexUpAction, 
	std::function<bool()> menuIndexDownAction)
{
	menuConditions_ = menuConditions;
	menuSelectAction_ = menuSelectAction;
	menuCancelAction_ = menuCancelAction;
	menuIndexUpAction_ = menuIndexUpAction;
	menuIndexDownAction_ = menuIndexDownAction;

	isMenuOpen_ = false;
	selectedIndex_ = 0;

	// メニューを閉じるアクションは常時有効にしておく
	menuActions_.clear();
	menuActions_.push_back([this]() { isMenuOpen_ = false; });
}

void MenuActionManager::Update()
{
	// メニューの表示条件を評価
	if(menuActions_.empty()) {
		isMenuOpen_ = false;
		return;
	}

	// メニューの選択アクションを評価
	if (menuConditions_ && menuConditions_()) {
		isMenuOpen_ = !isMenuOpen_;
	}

	// メニューが未来ていないなら早期リターン
	if (!isMenuOpen_) {
		return;
	}

	// メニューのキャンセルアクションを評価
	if (menuCancelAction_ && menuCancelAction_()) {
		isMenuOpen_ = false;
	}

	// メニューの上移動アクションを評価
	if (menuIndexUpAction_ && menuIndexUpAction_()) {
		selectedIndex_--;
		if (selectedIndex_ < 0) {
			selectedIndex_ = static_cast<int>(menuActions_.size()) - 1;
		}
	}
	if (menuIndexDownAction_ && menuIndexDownAction_()) {
		selectedIndex_++;
		if (selectedIndex_ >= static_cast<int>(menuActions_.size())) {
			selectedIndex_ = 0;
		}
	}

	// メニューの選択アクションを評価
	if (menuSelectAction_ && menuSelectAction_()) {
		if (selectedIndex_ >= 0 && selectedIndex_ < static_cast<int>(menuActions_.size())) {
			menuActions_[selectedIndex_]();
		}
	}
}
