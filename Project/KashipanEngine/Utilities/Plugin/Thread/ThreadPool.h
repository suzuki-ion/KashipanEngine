#pragma once
#include <vector>
#include <memory>
#include "ThreadWorker.h"

namespace Plugin {
	/// @brief 複数のThreadWorkerを管理するスレッドプールクラス
	class ThreadPool final {
	public:
		/// @brief Maxスレッドから-1した数のスレッドで初期化する
		ThreadPool();
		/// @brief デストラクタ
		~ThreadPool() = default;
		// コピーとムーブを禁止
		ThreadPool(const ThreadPool&) = delete;
		ThreadPool& operator=(const ThreadPool&) = delete;
		ThreadPool(ThreadPool&&) = delete;
		ThreadPool& operator=(ThreadPool&&) = delete;

		/// @brief タスクを空いているスレッドに追加する
		/// @param task 実行するタスク
		/// @return タスクの追加に成功したらtrueを返す
		bool AddTask(const std::function<void()>& task);

		/// @brief 空いているスレッドがあるかどうかを返す
		bool HasIdleThread() const;
		/// @brief スレッドの数を返す
		size_t GetThreadCount() const;

	private:
		/// @brief ThreadWorkerのリスト
		std::vector<std::unique_ptr<ThreadWorker>> workers_;
	};
}