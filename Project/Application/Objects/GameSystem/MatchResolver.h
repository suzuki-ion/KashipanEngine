#pragma once
#include <vector>
#include <stdint.h>

namespace Application
{
	class MatchResolver final
	{
	public:
		void Initialize();
		void Update(float delta);

		/// ブロックコンテナの状態を解析し、消えるブロックの位置を返します。
		std::vector<std::pair<int32_t, int32_t>> ResolveMatches(const std::vector<std::vector<int32_t>>& blockContainer);
		/// 4番のブロックに隣接したブロックが無いならば、その位置を返します。
		std::vector<std::pair<int32_t, int32_t>> ResolveIsolatedBlocks(const std::vector<std::vector<int32_t>>& blockContainer);
		/// 特定の位置から繋がっているブロックのリストを返します。
		std::vector<std::pair<int32_t, int32_t>> GetConnectedBlocks(const std::vector<std::vector<int32_t>>& blockContainer, int32_t startRow, int32_t startCol);
		/// 特定の位置を特定の種類から繋がっているブロックのリストを返します。
		std::vector<std::pair<int32_t, int32_t>> GetConnectedBlocksOfType(const std::vector<std::vector<int32_t>>& blockContainer, int32_t startRow, int32_t startCol, int32_t type);


		bool HaveMatch() const { return haveMatch_; }
		bool IsCompleteMatchStopFrame() const { return isCompleteMatchStopFrame_; }

	private:
		bool haveMatch_; // マッチがあるかどうかのフラグ
		float matchStopDuration_; // マッチがあるときにスクロールを停止する時間
		float matchStopTimer_; // マッチがあるときのタイマー
		bool isCompleteMatchStopFrame_; // マッチ停止が完了したかどうかのフラグ
	};
}
