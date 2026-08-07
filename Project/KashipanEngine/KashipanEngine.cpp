#include "KashipanEngine.h"
#include "EngineSettings.h"
#include "Debug/CrashHandler.h"
#include "Debug/LogSettings.h"
#include "Core/DirectX/ResourceLeakChecker.h"
#include "Core/GameEngine.h"
#include "Core/ProjectManager.h"
#include "Core/ProjectPaths.h"
#include "Core/UserSettings.h"
#include "Splash/SplashScreen.h"

#include "Utilities/Plugin/Plugins.h"
#include "Utilities/Translation.h"

namespace KashipanEngine {
namespace {
/// @brief スプラッシュ画面のRAIIガード
/// @details GameEngineのコンストラクタが例外を投げた場合でも、スコープを抜ける際に
///          確実にスプラッシュを閉じてスレッドを回収できるようにする
struct ScopedSplashScreen {
    explicit ScopedSplashScreen(PasskeyForGameEngineMain passkey) : passkey_(passkey) {
        SplashScreen::Show(passkey_);
    }
    ~ScopedSplashScreen() {
        SplashScreen::Close(passkey_);
    }
    PasskeyForGameEngineMain passkey_;
};
} // namespace

int Execute(PasskeyForWinMain winMainPasskey, const std::string &engineSettingsPath) {
    SetUnhandledExceptionFilter(CrashHandler);
    D3DResourceLeakChecker resourceLeakChecker;

    //--------- エンジン共通の翻訳の読み込み ---------//

    // プロジェクトを決める処理自体がログを出すため、それより先に翻訳を読み込む。
    // エンジンルートはプロジェクトと無関係に決まるので、ここまでは先に済ませられる
    ProjectPaths::InitializeEngineRootOnly(winMainPasskey);
    LoadGlobalTranslationFiles();

    //--------- 開くプロジェクトの決定 ---------//

    // エンジン設定ファイル自体がプロジェクト内にあるため、設定の読み込みより先に行う
    ProjectPaths::Initialize({});
    if (!ProjectManager::EnsureActiveProject({})) {
        assert(false && "Failed to open a project.");
        return -1;
    }

    //--------- 表示言語（個人設定）の適用 ---------//

    // UserSettingsはProjectPaths::Initialize以降でないと保存先が正しく解決できないため、
    // ここが最も早いタイミングになる。未設定（初回起動）の場合はOS既定の言語のままにする
    const std::string savedLanguage = UserSettings::GetString("editorUI.language", "");
    if (!savedLanguage.empty()) {
        SetCurrentLanguage(savedLanguage);
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

    // GameEngineのコンストラクタ（Window/DirectX12/各種マネージャ生成）が終わるまでは
    // 同期的にブロックするため、その間だけスプラッシュ画面を表示してログを流す
    std::unique_ptr<GameEngine> engine;
    {
        ScopedSplashScreen splashScreen{ PasskeyForGameEngineMain{} };
        engine = std::make_unique<GameEngine>(PasskeyForGameEngineMain{});
    }
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