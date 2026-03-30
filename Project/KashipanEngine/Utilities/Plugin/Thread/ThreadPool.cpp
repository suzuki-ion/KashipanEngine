#include "ThreadPool.h"
#include <KashipanEngine.h>

Plugin::ThreadPool::ThreadPool() {
	size_t threadCount = std::thread::hardware_concurrency();

	if (threadCount > 1) {
		threadCount -= 1; // メインスレッドを残す
	} else {
		threadCount = 1; // 最低1スレッドは確保
	}
	for (size_t i = 0; i < threadCount; ++i) {
		workers_.emplace_back(std::make_unique<ThreadWorker>());
	}
}

bool Plugin::ThreadPool::AddTask(const std::function<void()>& task) {
	KashipanEngine::LogScope scope;

	for (auto& worker : workers_) {
		if (!worker->IsBusy()) {
			worker->AddTask(task);
			return true;
		}
	}
	return false;
}

bool Plugin::ThreadPool::HasIdleThread() const {
	KashipanEngine::LogScope scope;

	for (const auto& worker : workers_) {
		if (!worker->IsBusy()) {
			return true;
		}
	}
	return false;
}

size_t Plugin::ThreadPool::GetThreadCount() const {
	return workers_.size();
}
