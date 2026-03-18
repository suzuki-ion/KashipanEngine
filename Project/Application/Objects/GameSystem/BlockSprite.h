#pragma once
#include <KashipanEngine.h>

namespace Application {
	// ブロックの状態をスプライトで描画するクラス
	class BlockSprite {
	public:
		/// @brief ブロックのスプライトを初期化する
		void Initialize(std::function<KashipanEngine::Sprite* (const std::string&, const std::string&, KashipanEngine::DefaultSampler)> createSpriteFunc);
		/// @brief ブロックの状態を更新する
		void Update();

		/// @brief ブロックの状態をスプライトに反映する
		void SetBlockState(int blockType);

		/// ブロックのスプライトをペアレントする
		void ParentTo(KashipanEngine::Sprite* parent);
		/// ブロックのスプライトの位置を設定する
		void SetPosition(const Vector3& position);
		/// ブロックのスプライトのサイズを取得する
		float GetSize() const;
		/// ブロックをある地点へ移動させる
		void MoveTo(const Vector3& targetPosition, float transitionSpeed);
		/// 移動量分ブロックを移動させる
		void AddMove(const Vector3& deltaPosition);

	private:
		KashipanEngine::Sprite* blockSprite_ = nullptr;
	};
}