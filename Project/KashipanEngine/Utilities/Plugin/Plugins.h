#pragma once
#include "Thread/ThreadPool.h"
#include "Thread/PriorityTaskDispatcher.h"

namespace Plugin {
	inline std::function<void(const std::function<void()>&, int)> addAsyncTask;
	inline std::function<void()> executeAsyncTasks;
	inline std::function<bool()> hasAsyncTasks;
}