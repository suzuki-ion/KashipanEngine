#include "KashipanEngine.h"
#include "EngineSettings.h"
#include "Debug/CrashHandler.h"
#include "Debug/LogSettings.h"
#include "Core/DirectX/ResourceLeakChecker.h"
#include "Core/GameEngine.h"
#include "Core/ProjectManager.h"
#include "Core/PlayerSettings.h"
#include "Core/ProjectPaths.h"
#include "Core/UserSettings.h"
#include "Splash/SplashScreen.h"

#include "Utilities/Plugin/Plugins.h"
#include "Utilities/Conversion/ConvertString.h"
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
    try {
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
        MessageBoxW(nullptr,
            L"起動するプロジェクトを見つけられませんでした。\nProject.jsonまたはAssetsフォルダーを確認してください。",
            L"KashipanEngine 起動エラー", MB_OK | MB_ICONERROR);
        return -1;
    }

    //--------- 表示言語（個人設定）の適用 ---------//

    // UserSettings/PlayerSettingsはProjectPaths::Initialize以降でないと保存先が正しく解決できないため、
    // ここが最も早いタイミングになる。未設定（初回起動）の場合はOS既定の言語のままにする。
    // エディター側（UserSettings）とアプリケーション側（PlayerSettings）の表示言語は互いに
    // 独立しているため、それぞれ別の設定値を別の状態へ適用する
    const std::string savedEditorLanguage = UserSettings::GetString("editorUI.language", "");
    if (!savedEditorLanguage.empty()) {
        SetCurrentLanguage(savedEditorLanguage);
    }
    const std::string savedApplicationLanguage = PlayerSettings::GetString(PlayerSettings::kLanguageKey, "");
    if (!savedApplicationLanguage.empty()) {
        SetCurrentApplicationLanguage(savedApplicationLanguage);
    }

    //--------- 設定ファイル読み込み ---------//

    const std::string resolvedEngineSettingsPath = ProjectPaths::ToPhysical(engineSettingsPath);
    JSON engineSettingsJSON = LoadJSON(resolvedEngineSettingsPath);
    if (engineSettingsJSON.is_null()) {
        assert(false && "Failed to load engine settings JSON.");
        const std::wstring message = L"エンジン設定を読み込めませんでした。\n\n" +
            ConvertString(resolvedEngineSettingsPath);
        MessageBoxW(nullptr, message.c_str(), L"KashipanEngine 起動エラー", MB_OK | MB_ICONERROR);
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
    } catch (const std::exception &exception) {
        const std::string detail = exception.what();
        Log("Fatal startup/runtime error: " + detail, LogSeverity::Critical);
        ShutdownLogger({});
        const std::wstring message =
            L"KashipanEngineを起動できませんでした。\n\n詳細: " + ConvertString(detail) +
            L"\n\nLogsフォルダーのログも確認してください。";
        MessageBoxW(nullptr, message.c_str(), L"KashipanEngine 起動エラー", MB_OK | MB_ICONERROR);
        return -1;
    } catch (...) {
        Log("Fatal startup/runtime error: unknown exception", LogSeverity::Critical);
        ShutdownLogger({});
        MessageBoxW(nullptr,
            L"KashipanEngineを起動できませんでした。\n\n不明な例外が発生しました。",
            L"KashipanEngine 起動エラー", MB_OK | MB_ICONERROR);
        return -1;
    }
}

} // namespace KashipanEngine
