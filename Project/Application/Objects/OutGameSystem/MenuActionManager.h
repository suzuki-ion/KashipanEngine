#pragma once
#include <functional>
#include <vector>

namespace Application
{
	/// メニューの開閉、表示内容、選択肢の管理を行うクラス
	class MenuActionManager
	{
	public:
		/// メニューシステムを初期化する
		/// @param menuConditions メニューの表示条件を返す関数
		/// @param menuSelectAction メニューの選択アクションを返す
		/// @param menuCancelAction メニューのキャンセルアクションを返す
		/// @param menuIndexUpAction メニューの上移動アクションを
		/// @param menuIndexDownAction メニューの下移動アクションを返す
		void Initialize(
			std::function<bool()> menuConditions,
			std::function<bool()> menuSelectAction,
			std::function<bool()> menuCancelAction,
			std::function<bool()> menuIndexUpAction,
			std::function<bool()> menuIndexDownAction);
		/// メニューシステムを更新する
		void Update();

		/// メニューのアクションを追加する
		void AddMenuAction(std::function<void()> action) { menuActions_.push_back(action); }

		/// メニューが開いているかどうかを取得する
		bool IsMenuOpen() const { return isMenuOpen_; }
		/// メニューで選択されているインデックスを取得する
		int GetSelectedIndex() const { return selectedIndex_; }
		/// メニューの選択肢数を取得する
		int GetMenuActionCount() const { return static_cast<int>(menuActions_.size()); }

		/// メニューの開閉を設定する
		void SetMenuOpen(bool isOpen) { isMenuOpen_ = isOpen; }

	private:
		bool isMenuOpen_; // メニューが開いているかどうか
		std::vector<std::function<void()>> menuActions_; // メニューのアクションリスト
		std::function<bool()> menuConditions_; // メニューの表示条件リスト
		std::function<bool()> menuSelectAction_; // メニューの選択アクション
		std::function<bool()> menuCancelAction_; // メニューのキャンセルアクション
		std::function<bool()> menuIndexUpAction_; // メニューの上移動アクション
		std::function<bool()> menuIndexDownAction_; // メニューの下移動アクション

		int selectedIndex_; // 現在選択されているメニューのインデックス
	};
}