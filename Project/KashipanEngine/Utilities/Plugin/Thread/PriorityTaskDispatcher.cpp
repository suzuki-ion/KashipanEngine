#include "PriorityTaskDispatcher.h"
#include <algorithm>
#include <KashipanEngine.h>

Plugin::PriorityTaskDispatcher::PriorityTaskDispatcher(
	std::function<bool(const std::function<void()>&)> addTaskFunc,
	std::function<bool()> hasIdleWorker) :
	addTaskFunc_(addTaskFunc),
	hasIdleWorker_(hasIdleWorker) {
}

void Plugin::PriorityTaskDispatcher::AddTask(const std::function<void()>& task, int priority) {
	KashipanEngine::LogScope scope;

	// タスクを追加
	tasks_.push_back({ task, priority });
	// 優先度順にソート
	std::sort(tasks_.begin(), tasks_.end(),
		[](const Task& a, const Task& b) {
			return a.priority < b.priority;
		});
}

void Plugin::PriorityTaskDispatcher::ExecuteTasks() {
	KashipanEngine::LogScope scope;

	while (!tasks_.empty() && hasIdleWorker_()) {
		// 最も優先度の高いタスクを取得
		Task task = tasks_.front();
		tasks_.erase(tasks_.begin());
		// タスクを追加
		addTaskFunc_(task.func);
	}
}

bool Plugin::PriorityTaskDispatcher::HasTasks() const {
	return !tasks_.empty();
}
