#pragma once
#include <KashipanEngine.h>
#include <Objects/OutGameSystem/ControllerViewer.h>

namespace Application {
	/// メニューのスプライトを管理するクラス
	class MenuSpriteContainer final {
	public:
		/// メニューのスプライトを初期化します。
		void Initialize(
			std::function<KashipanEngine::Sprite* (const std::string&, const std::string&)> createSpriteFunc);

		/// メニューのスプライトの状態を更新します。
		void Update(float delta);

		/// メニューのスプライトの位置を一括で設定します。positionはメニューの中心位置になります。
		void SetPosition(const Vector2& position);
		/// 選択されているメニューのインデックスを設定します。
		void SetSelectedIndex(int index);
		/// メニューのスプライトを設定します。
		void SetMenuSprite(std::vector<KashipanEngine::Sprite*> sprite);
		/// メニューの開閉を伝えます
		void SetMenuOpen(bool isOpen) { isMenuOpen_ = isOpen; }

		// コントローラーの入力を可視化するオブジェクトを取得します
		ControllerViewer* GetControllerViewer() { return &controllerViewer_; }

	private:
		std::function<KashipanEngine::Sprite* (const std::string&, const std::string&)> createSpriteFunc_;
		std::vector<KashipanEngine::Sprite*> menuSprites_;
		std::vector<KashipanEngine::Sprite*> menuTextSprites_;
		KashipanEngine::Sprite* menuBGSprite_ = nullptr;
		float timer_ = 0.0f;

		int selectedIndex_;
		Vector2 menuAnchorPoint_;
		float rotateOffset_;
		float targetRotateOffset_;
		bool isMenuOpen_;
		Vector2 screenCenter_;

		int menuSpriteCount_;

		// コントローラーの入力を可視化するオブジェクト
		ControllerViewer controllerViewer_;
	};
}