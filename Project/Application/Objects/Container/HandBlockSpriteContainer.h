#pragma once
#include <KashipanEngine.h>

namespace Application {
	/// 手持ちのブロックのスプライトを管理するクラス
	class HandBlockSpriteContainer final {
	public:
		/// 指定された最大ブロック数で初期化します。
		void Initialize(int32_t maxBlocks);
		/// ブロックのスプライトの位置を一括で設定します。positionは手持ちブロックの中心位置になります。
		void SetPosition(const Vector2& position);

		/// 特定のインデックスのスプライトを設定します。
		void SetHandBlockSprite(int32_t index, KashipanEngine::Sprite* sprite);
		/// 特定のインデックスのスプライトを取得します。範囲外の場合はnullptrを返します。
		KashipanEngine::Sprite* GetHandBlockSprite(int32_t index) const;
		/// 特定のインデックスのスプライトの色を設定します。
		void SetHandBlockSpriteColor(int32_t index, const Vector4& color);

	private:
		int32_t maxBlocks_;
		std::vector<KashipanEngine::Sprite*> handBlockSprites_;
	};
}