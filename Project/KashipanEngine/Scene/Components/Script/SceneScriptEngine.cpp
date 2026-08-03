#include "Scene/Components/Script/SceneScriptEngine.h"

#include <angelscript.h>
#include <add_on/scriptarray/scriptarray.h>
#include <add_on/scriptstdstring/scriptstdstring.h>
#include <add_on/scriptdictionary/scriptdictionary.h>
#include <add_on/scripthelper/scripthelper.h>

#include "Debug/Logger.h"
#include "Scene/Components/Script/AngelScriptDebugServer.h"
#include "Core/ProjectPaths.h"
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

} // namespace

SceneScriptEngine::~SceneScriptEngine() = default;

void SceneScriptEngine::Initialize() {
    if (engine_) return;

    engine_ = asCreateScriptEngine();
    if (!engine_) {
        Log(Translation("engine.script.failed.createengine"), LogSeverity::Error);
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
    auto &debugServer = GetProcessAngelScriptDebugServer();
    if (debugServer.Start()) {
        debugServer_ = &debugServer;
    }
#endif

#if !defined(RELEASE_BUILD)
    // VSCodeのAngelScript Language Server用の型定義ファイルを生成する（Releaseビルドでは生成しない）
    if (GenerateScriptPredefinedFile(engine_, ProjectPaths::InProjectRoot("as.predefined"))) {
        Log(Translation("engine.script.predefined.generated"));
    } else {
        Log(Translation("engine.script.predefined.generate.failed"), LogSeverity::Warning);
    }
#endif
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
    ImGui::Text("%s%d", TranslationC("editor.scriptengine.version"), ANGELSCRIPT_VERSION);
    ImGui::Text("%s%s", TranslationC("editor.scriptengine.engine"),
        engine_ ? TranslationC("initialized") : TranslationC("editor.scriptengine.notinitialized"));
    if (engine_) {
        ImGui::Text("%s%d", TranslationC("editor.scriptengine.modules"), static_cast<int>(engine_->GetModuleCount()));
    }
}
#endif

} // namespace KashipanEngine
