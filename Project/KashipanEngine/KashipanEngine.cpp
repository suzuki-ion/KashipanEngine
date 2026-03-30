#include "KashipanEngine.h"
#include "EngineSettings.h"
#include "Debug/CrashHandler.h"
#include "Debug/LogSettings.h"
#include "Core/DirectX/ResourceLeakChecker.h"
#include "Core/GameEngine.h"

#include "Utilities/Plugin/Plugins.h"

namespace KashipanEngine {
int Execute(PasskeyForWinMain, const std::string &engineSettingsPath) {
    SetUnhandledExceptionFilter(CrashHandler);
    D3DResourceLeakChecker resourceLeakChecker;

    //--------- 設定ファイル読み込み ---------//

    JSON engineSettingsJSON = LoadJSON(engineSettingsPath);
    if (engineSettingsJSON.is_null()) {
        assert(false && "Failed to load engine settings JSON.");
        return -1;
    }
    std::string logSettingsPath = engineSettingsJSON.value("logSettingsPath", "LogSettings.json");
    LoadLogSettings({}, logSettingsPath);
    InitializeLogger({});
    LoadEngineSettings({}, engineSettingsPath);

	// --------- プラグインの初期化 ---------//
	Plugin::ThreadPool threadPool;
	Plugin::PriorityTaskDispatcher taskDispatcher(
		[&threadPool](const std::function<void()>& task) {
			return threadPool.AddTask(task);
		},
		[&threadPool]() {
			return threadPool.HasIdleThread();
		}
	);
    Plugin::addAsyncTask = [&taskDispatcher](const std::function<void()>& task,int priority) {
        taskDispatcher.AddTask(task, priority);
		};
	Plugin::executeAsyncTasks = [&taskDispatcher]() {
		taskDispatcher.ExecuteTasks();
		};
	Plugin::hasAsyncTasks = [&taskDispatcher]() {
		return taskDispatcher.HasTasks();
		};

    //--------- エンジン実行 ---------//

    std::unique_ptr<GameEngine> engine = std::make_unique<GameEngine>(PasskeyForGameEngineMain{});
    int code = engine->Execute({});

    //--------- エンジン終了 ---------//

    engine.reset();
    ShutdownLogger({});
    return code;
}

} // namespace KashipanEngine