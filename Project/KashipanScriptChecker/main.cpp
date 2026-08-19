#include <angelscript.h>
#include <add_on/scriptarray/scriptarray.h>
#include <add_on/scriptstdstring/scriptstdstring.h>
#include <add_on/scriptdictionary/scriptdictionary.h>
#include <add_on/scripthelper/scripthelper.h>
#include <add_on/scriptbuilder/scriptbuilder.h>

#include <cstdio>
#include <string>

#include "Scene/Components/Script/ScriptBindings.h"

using namespace KashipanEngine;

namespace {

bool gHadError = false;

/// @brief AngelScriptのコンパイルメッセージをstderrへ出力する
void MessageCallback(const asSMessageInfo *msg, void *) {
    const char *severityLabel = "Info";
    if (msg->type == asMSGTYPE_ERROR) {
        severityLabel = "Error";
        gHadError = true;
    } else if (msg->type == asMSGTYPE_WARNING) {
        severityLabel = "Warning";
    }
    std::fprintf(stderr, "[%s] %s(%d, %d): %s\n", severityLabel, msg->section, msg->row, msg->col, msg->message);
}

/// @brief `#include "path.as"` を解決するコールバック（ScriptComponent::ResolveIncludePathと同じ規則）
int ResolveIncludePath(const char *include, const char *from, CScriptBuilder *builder, void *) {
    if (!include || !builder) return -1;

    std::string includePath = include;
    const bool isAbsolute = includePath.size() >= 2 &&
        (includePath[1] == ':' || includePath[0] == '/' || includePath[0] == '\\');

    if (!isAbsolute && from) {
        const std::string fromPath = from;
        const auto slashPos = fromPath.find_last_of("/\\");
        if (slashPos != std::string::npos) {
            includePath = fromPath.substr(0, slashPos + 1) + includePath;
        }
    }
    return builder->AddSectionFromFile(includePath.c_str());
}

} // namespace

/// @brief KashipanEngineを起動せずAngelScriptのコンパイルチェックだけを行うCLIツール
/// @details 引数に渡した .as ファイルそれぞれを独立したモジュールとしてビルドし、
///          失敗したものがあれば終了コード1を返す。実行(Execute)は一切行わないため、
///          DirectXデバイスやウィンドウが無い環境でも動作する。
int main(int argc, char **argv) {
    if (argc < 2) {
        std::fprintf(stderr, "usage: KashipanScriptChecker.exe <script.as> [script2.as ...]\n");
        return 1;
    }

    asIScriptEngine *engine = asCreateScriptEngine();
    if (!engine) {
        std::fprintf(stderr, "[Error] failed to create AngelScript engine\n");
        return 1;
    }
    engine->SetMessageCallback(asFUNCTION(MessageCallback), nullptr, asCALL_CDECL);
    RegisterScriptArray(engine, true);
    RegisterStdString(engine);
    RegisterScriptDictionary(engine);
    RegisterExceptionRoutines(engine);
    RegisterEngineScriptBindings(engine);

    bool allOk = true;
    for (int i = 1; i < argc; ++i) {
        gHadError = false;
        const std::string path = argv[i];
        const std::string moduleName = "Checker_" + std::to_string(i);

        CScriptBuilder builder;
        if (builder.StartNewModule(engine, moduleName.c_str()) < 0) {
            std::fprintf(stderr, "[Error] failed to create module for: %s\n", path.c_str());
            allOk = false;
            continue;
        }
        builder.SetIncludeCallback(ResolveIncludePath, nullptr);

        const bool sectionLoaded = builder.AddSectionFromFile(path.c_str()) >= 0;
        const bool built = sectionLoaded && builder.BuildModule() >= 0;

        if (!sectionLoaded) {
            std::fprintf(stderr, "[Error] failed to read script file: %s\n", path.c_str());
            allOk = false;
        } else if (!built || gHadError) {
            std::fprintf(stderr, "[Error] build failed: %s\n", path.c_str());
            allOk = false;
        } else {
            std::fprintf(stdout, "[OK] %s\n", path.c_str());
        }
        engine->DiscardModule(moduleName.c_str());
    }

    engine->ShutDownAndRelease();
    return allOk ? 0 : 1;
}
