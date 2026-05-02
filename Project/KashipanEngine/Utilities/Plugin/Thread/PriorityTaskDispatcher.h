#pragma once
#include <functional>
#include <vector>

namespace Plugin {
	/// @brief 優先度付きタスクを管理、並列実行するクラス
	class PriorityTaskDispatcher final {
	public:
		/// @brief デフォルトコンストラクタを禁止
		PriorityTaskDispatcher() = delete;
		explicit PriorityTaskDispatcher(
			std::function<bool(const std::function<void()>&)> addTaskFunc,
			std::function<bool()>hasIdleWorker);
		~PriorityTaskDispatcher() = default;
		// コピーとムーブを禁止
		PriorityTaskDispatcher(const PriorityTaskDispatcher&) = delete;
		PriorityTaskDispatcher& operator=(const PriorityTaskDispatcher&) = delete;
		PriorityTaskDispatcher(PriorityTaskDispatcher&&) = delete;
		PriorityTaskDispatcher& operator=(PriorityTaskDispatcher&&) = delete;

		/// @brief 優先度付きタスクを追加する
		/// @param task 実行するタスク
		/// @param priority タスクの優先度（数値が小さいほど高優先度）
		void AddTask(const std::function<void()>& task, int priority);
		/// @brief 追加されたタスクを優先度順に実行する
		void ExecuteTasks();

		/// @brief タスクが存在するかどうかを返す
		bool HasTasks() const;

	private:
		std::function<bool(const std::function<void()>&)> addTaskFunc_;
		std::function<bool()> hasIdleWorker_;

		struct Task {
			std::function<void()> func;
			int priority;
		};
		std::vector<Task> tasks_;
	};
}