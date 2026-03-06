#pragma once
#include <vector>
#include <stdint.h>

namespace Application {
	/// 手持ちのブロックを管理するクラス
	class HandBlockContainer final {
	public:
		void Initialize(int32_t maxBlocks);
		
		int32_t GetMaxBlocks() const { return maxBlocks_; }
		std::vector<int32_t> GetHandBlocks() const { return handBlocks_; }
		int32_t PopHandBlock();
		void PopFront();

		void ResetReloadTimer() { reloadTimer_ = maxReloadTime_; }
		void ReloadBlocks(float delta);

		bool HaveHandBlocks() const {
			for (int block : handBlocks_) {
				if (block != 0) {
					return true;
				}
			}
			return false;
		}

	private:
		float reloadTimer_ = 0.0f; // 手持ちのブロックをリロードするためのタイマー
		float maxReloadTime_ = 0.5f; // 手持ちのブロックをリロードするのにかかる時間
		int32_t maxBlocks_;
		std::vector<int32_t> handBlocks_;
	};
}