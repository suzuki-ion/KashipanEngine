#include "KashipanEngine.h"
#include "EngineSettings.h"
#include "Debug/CrashHandler.h"
#include "Debug/LogSettings.h"
#include "Core/DirectX/ResourceLeakChecker.h"
#include "Core/GameEngine.h"
#include "Core/ProjectManager.h"
#include "Core/ProjectPaths.h"

#include "Utilities/Plugin/Plugins.h"

namespace KashipanEngine {
int Execute(PasskeyForWinMain, const std::string &engineSettingsPath) {
    SetUnhandledExceptionFilter(CrashHandler);
    D3DResourceLeakChecker resourceLeakChecker;

    //--------- 開くプロジェクトの決定 ---------//

    // エンジン設定ファイル自体がプロジェクト内にあるため、設定の読み込みより先に行う
    ProjectPaths::Initialize({});
    if (!ProjectManager::EnsureActiveProject({})) {
        assert(false && "Failed to open a project.");
        return -1;
    }

    //--------- 設定ファイル読み込み ---------//

    const std::string resolvedEngineSettingsPath = ProjectPaths::ToPhysical(engineSettingsPath);
    JSON engineSettingsJSON = LoadJSON(resolvedEngineSettingsPath);
    if (engineSettingsJSON.is_null()) {
        assert(false && "Failed to load engine settings JSON.");
        return -1;
    }
    std::string logSettingsPath = engineSettingsJSON.value("logSettingsPath", "LogSettings.json");
    LoadLogSettings({}, ProjectPaths::ToPhysical(logSettingsPath));
    InitializeLogger({});
    LoadEngineSettings({}, resolvedEngineSettingsPath);

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

    // プロジェクトの切り替えが要求されていた場合は、シーンの自動保存など後始末が
    // すべて終わったこの時点で新しいプロセスを起動する
    ProjectManager::LaunchPendingRestart({});

    ShutdownLogger({});
    return code;
}

} // namespace KashipanEngine