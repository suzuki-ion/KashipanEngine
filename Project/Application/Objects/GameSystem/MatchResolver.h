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

		bool HaveMatch() const { return haveMatch_; }
		bool IsCompleteMatchStopFrame() const { return isCompleteMatchStopFrame_; }

	private:
		bool haveMatch_; // マッチがあるかどうかのフラグ
		float matchStopDuration_; // マッチがあるときにスクロールを停止する時間
		float matchStopTimer_; // マッチがあるときのタイマー
		bool isCompleteMatchStopFrame_; // マッチ停止が完了したかどうかのフラグ
	};
}
