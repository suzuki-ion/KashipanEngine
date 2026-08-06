#pragma once
#include <atomic>
#include <chrono>
#include <cstddef>
#include <thread>
#include "Thread/ThreadPool.h"
#include "Thread/PriorityTaskDispatcher.h"

namespace Plugin {
	inline std::function<void(const std::function<void()>&, int)> addAsyncTask;
	inline std::function<void()> executeAsyncTasks;
	inline std::function<bool()> hasAsyncTasks;

	/// @brief count個のタスクをスレッドプールで並列実行し、すべて完了するまで呼び出し元をブロックして待機する
	/// @details addAsyncTask/executeAsyncTasksはタスクをワーカーに"投入"するだけで実行完了までは
	///          追跡しないため、完了通知用のカウンタを自前で持って待ち合わせる
	/// @param count 実行するタスク数
	/// @param taskForIndex インデックス（0～count-1）を受け取り実行するタスク本体。
	///        複数のワーカースレッドから並列に呼ばれるため、スレッドセーフに実装すること
	/// @param priority タスクの優先度（数値が小さいほど高優先度）
	inline void RunParallelAndWait(size_t count, const std::function<void(size_t)>& taskForIndex, int priority = 0) {
		if (count == 0) return;

		std::atomic<size_t> remaining{ count };
		for (size_t i = 0; i < count; ++i) {
			addAsyncTask([&taskForIndex, &remaining, i]() {
				taskForIndex(i);
				remaining.fetch_sub(1, std::memory_order_acq_rel);
				}, priority);
		}

		// タスクディスパッチャは空いたワーカーへ随時流し込む方式のため、
		// 全タスク完了までexecuteAsyncTasksを呼び続けてポーリングする
		while (remaining.load(std::memory_order_acquire) != 0) {
			executeAsyncTasks();
			std::this_thread::sleep_for(std::chrono::microseconds(200));
		}
	}
}