#include "Scene/Components/Script/SceneScriptEngine.h"

#include <angelscript.h>
#include <add_on/scriptarray/scriptarray.h>
#include <add_on/scriptstdstring/scriptstdstring.h>
#include <add_on/scriptdictionary/scriptdictionary.h>
#include <add_on/scripthelper/scripthelper.h>

#include "Debug/Logger.h"
#include "Scene/Components/Script/AngelScriptDebugServer.h"
#include "Scene/Components/Script/ScriptBindings.h"

namespace KashipanEngine {

namespace {

/// @brief メッセージ収集先（BeginMessageCapture～EndMessageCaptureの間だけ設定される）
std::vector<std::string> *gActiveMessageCapture = nullptr;

void MessageCallback(const asSMessageInfo *msg, void *param) {
    (void)param;
    LogSeverity severity = LogSeverity::Info;
    const char *severityLabel = "[Info]";
    if (msg->type == asMSGTYPE_ERROR) {
        severity = LogSeverity::Error;
        severityLabel = "[Error]";
    } else if (msg->type == asMSGTYPE_WARNING) {
        severity = LogSeverity::Warning;
        severityLabel = "[Warning]";
    }
    const std::string text = std::string(msg->section) + "(" + std::to_string(msg->row) + ", " + std::to_string(msg->col) + "): " + msg->message;
    Log(text, severity);
    if (gActiveMessageCapture) {
        gActiveMessageCapture->push_back(std::string(severityLabel) + " " + text);
    }
}

#if !defined(RELEASE_BUILD)
/// @brief シーン切り替え中に複数のSceneScriptEngineが重なっても接続を維持できるプロセス共通サーバー
AngelScriptDebugServer &GetProcessDebugServer() {
    static AngelScriptDebugServer server;
    return server;
}
#endif

} // namespace

SceneScriptEngine::~SceneScriptEngine() = default;

void SceneScriptEngine::Initialize() {
    if (engine_) return;

    engine_ = asCreateScriptEngine();
    if (!engine_) {
        Log("AngelScript: asCreateScriptEngine に失敗しました", LogSeverity::Error);
        return;
    }
    engine_->SetMessageCallback(asFUNCTION(MessageCallback), nullptr, asCALL_CDECL);
    RegisterScriptArray(engine_, true);
    RegisterStdString(engine_);
    // 辞書型（dictionary）。string/arrayを使用するため両者の登録後に呼ぶ
    RegisterScriptDictionary(engine_);
    RegisterExceptionRoutines(engine_);
    RegisterEngineScriptBindings(engine_);

#if !defined(RELEASE_BUILD)
    auto &debugServer = GetProcessDebugServer();
    if (debugServer.Start()) {
        debugServer_ = &debugServer;
    }
#endif

    // VSCodeのAngelScript Language Server用の型定義ファイルを生成する
    if (GenerateScriptPredefinedFile(engine_, "as.predefined")) {
        Log("AngelScript: as.predefined を生成しました");
    } else {
        Log("AngelScript: as.predefined の生成に失敗しました", LogSeverity::Warning);
    }
}

void SceneScriptEngine::AttachDebugger(asIScriptContext *context) const {
    if (debugServer_) {
        debugServer_->AttachContext(context);
    }
}

void SceneScriptEngine::BeginMessageCapture() {
    messageCaptureBuffer_.clear();
    gActiveMessageCapture = &messageCaptureBuffer_;
}

std::vector<std::string> SceneScriptEngine::EndMessageCapture() {
    gActiveMessageCapture = nullptr;
    return std::move(messageCaptureBuffer_);
}

void SceneScriptEngine::Finalize() {
    if (gActiveMessageCapture == &messageCaptureBuffer_) {
        gActiveMessageCapture = nullptr;
    }
    debugServer_ = nullptr;
    if (engine_) {
        engine_->ShutDownAndRelease();
        engine_ = nullptr;
    }
}

#if defined(USE_IMGUI)
void SceneScriptEngine::ShowImGui() {
    ImGui::Text("AngelScript Version: %d", ANGELSCRIPT_VERSION);
    ImGui::Text("Engine: %s", engine_ ? "Initialized" : "Not Initialized");
    if (engine_) {
        ImGui::Text("Modules: %d", static_cast<int>(engine_->GetModuleCount()));
    }
}
#endif

} // namespace KashipanEngine
