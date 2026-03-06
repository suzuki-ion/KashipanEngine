#pragma once
#include <KashipanEngine.h>

namespace Application {
	/// メニューのスプライトを管理するクラス
	class MenuSpriteContainer final {
	public:
		/// メニューのスプライトを初期化します。
		void Initialize();

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

	private:
		std::vector<KashipanEngine::Sprite*> menuSprites_;
		std::vector<Vector2> menuSpriteOffsets_; // メニューのスプライトのオフセット位置

		int selectedIndex_;
		Vector2 menuAnchorPoint_;
		float rotateOffset_;
		float targetRotateOffset_;
		bool isMenuOpen_;
	};
}