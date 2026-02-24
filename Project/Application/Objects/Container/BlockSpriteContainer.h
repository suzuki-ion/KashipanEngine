#pragma once
#include <KashipanEngine.h>

namespace Application {
	/// ブロックのスプライトに対する操作を管理するクラス
	class BlockSpriteContainer final {
	public:
		/// 指定された行数と列数で初期化します。
		void Initialize(int32_t rows, int32_t cols);
		
		/// 特定の行列のスプライトを設定します。
		void SetBlockSprite(int32_t row, int32_t col, KashipanEngine::Sprite* sprite);
		/// 特定の行列のスプライトを取得します。範囲外の場合はnullptrを返します。
		KashipanEngine::Sprite* GetBlockSprite(int32_t row, int32_t col) const;

		/// ブロックのスプライトの位置を一括で設定します。positionはブロックコンテナの中心位置になります。
		void RsetBlockPosition(const Vector2& position, const Vector2& size);
		/// ブロックのスプライトの色を一括で設定します。
		void SetBlockSpriteColor(int32_t row, int32_t col, const Vector4& color);

		void SetBlockSpriteScale(int32_t row, int32_t col, const Vector2& scale);

	private:
		std::vector<std::vector<KashipanEngine::Sprite*>> blockSprites_;
	};
}
