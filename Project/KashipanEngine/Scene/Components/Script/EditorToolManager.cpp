#include "Scene/Components/Script/EditorToolManager.h"
#ifdef USE_IMGUI

#include <functional>

#include <angelscript.h>
#include <add_on/scriptarray/scriptarray.h>
#include <add_on/scriptbuilder/scriptbuilder.h>
#include <add_on/scriptdictionary/scriptdictionary.h>
#include <add_on/scripthelper/scripthelper.h>
#include <add_on/scriptstdstring/scriptstdstring.h>

#include <imgui.h>

#include "Debug/Logger.h"
#include "Scene/Components/Script/ImGuiScriptBindings.h"
#include "Scene/Components/Script/ScriptBindings.h"
#include "Utilities/FileIO/Directory.h"

namespace KashipanEngine {

namespace {

constexpr const char *kEditorToolsDirectory = "EditorTools";
constexpr const char *kEditorToolInterfaceName = "EditorTool";

void EditorToolMessageCallback(const asSMessageInfo *msg, void *param) {
    (void)param;
    LogSeverity severity = LogSeverity::Info;
    if (msg->type == asMSGTYPE_ERROR) {
        severity = LogSeverity::Error;
    } else if (msg->type == asMSGTYPE_WARNING) {
        severity = LogSeverity::Warning;
    }
    const std::string text = std::string("EditorTool: ") + msg->section + "(" + std::to_string(msg->row) + ", " + std::to_string(msg->col) + "): " + msg->message;
    Log(text, severity);
}

/// @brief `#include "path.as"` を解決するコールバック（ScriptComponentと同じ解決規則）
int ResolveEditorToolIncludePath(const char *include, const char *from, CScriptBuilder *builder, void *) {
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

/// @brief 属性メタデータのトークン（属性名 + 引数リスト）
struct ToolAttributeToken final {
    std::string name;
    std::vector<std::string> args;
};

std::string TrimToolMetadata(const std::string &metadata) {
    const auto first = metadata.find_first_not_of(" \t");
    if (first == std::string::npos) return {};
    const auto last = metadata.find_last_not_of(" \t");
    return metadata.substr(first, last - first + 1);
}

std::string UnquoteToolAttributeArg(const std::string &arg) {
    std::string s = TrimToolMetadata(arg);
    if (s.size() >= 2 && ((s.front() == '"' && s.back() == '"') || (s.front() == '\'' && s.back() == '\''))) {
        s = s.substr(1, s.size() - 2);
    }
    return s;
}

/// @brief 1つのメタデータブロック文字列を属性トークンへ分解する
/// @details `[EditorWindow("..."), MenuItem("...", "...")]` のように1ブロックへ複数属性を書けるため、
///          括弧・引用符の外にあるカンマでのみ分割する（ScriptComponentの属性解析と同じ規則）
void ParseToolMetadataString(const std::string &metadata, std::vector<ToolAttributeToken> &out) {
    std::vector<std::string> parts;
    std::string current;
    int parenDepth = 0;
    bool inQuote = false;
    char quoteChar = '"';
    for (const char c : metadata) {
        if (inQuote) {
            if (c == quoteChar) inQuote = false;
            current += c;
            continue;
        }
        if (c == '"' || c == '\'') { inQuote = true; quoteChar = c; current += c; continue; }
        if (c == '(') { ++parenDepth; current += c; continue; }
        if (c == ')') { if (parenDepth > 0) --parenDepth; current += c; continue; }
        if (c == ',' && parenDepth == 0) { parts.push_back(current); current.clear(); continue; }
        current += c;
    }
    parts.push_back(current);

    for (const auto &part : parts) {
        const std::string token = TrimToolMetadata(part);
        if (token.empty()) continue;

        ToolAttributeToken attr;
        const auto open = token.find('(');
        if (open == std::string::npos) {
            attr.name = token;
        } else {
            attr.name = TrimToolMetadata(token.substr(0, open));
            const auto close = token.rfind(')');
            const auto argsEnd = (close != std::string::npos && close > open) ? close : token.size();
            const std::string argsStr = token.substr(open + 1, argsEnd - open - 1);

            if (!TrimToolMetadata(argsStr).empty()) {
                std::string arg;
                bool argInQuote = false;
                char argQuote = '"';
                for (const char c : argsStr) {
                    if (argInQuote) {
                        if (c == argQuote) argInQuote = false;
                        arg += c;
                        continue;
                    }
                    if (c == '"' || c == '\'') { argInQuote = true; argQuote = c; arg += c; continue; }
                    if (c == ',') { attr.args.push_back(UnquoteToolAttributeArg(arg)); arg.clear(); continue; }
                    arg += c;
                }
                attr.args.push_back(UnquoteToolAttributeArg(arg));
            }
        }
        out.push_back(std::move(attr));
    }
}

std::vector<ToolAttributeToken> ParseToolAttributeTokens(const std::vector<std::string> &metadataList) {
    std::vector<ToolAttributeToken> tokens;
    for (const auto &metadata : metadataList) {
        ParseToolMetadataString(metadata, tokens);
    }
    return tokens;
}

/// @brief パスを '/' で分割する（空セグメントは除外）
std::vector<std::string> SplitMenuPath(const std::string &path) {
    std::vector<std::string> segments;
    std::string current;
    for (const char c : path) {
        if (c == '/') {
            if (!current.empty()) segments.push_back(current);
            current.clear();
        } else {
            current += c;
        }
    }
    if (!current.empty()) segments.push_back(current);
    return segments;
}

} // namespace

EditorToolManager &EditorToolManager::GetInstance() {
    static EditorToolManager instance;
    return instance;
}

EditorToolManager::~EditorToolManager() {
    for (auto &tool : tools_) {
        if (tool.object) tool.object->Release();
    }
    tools_.clear();
    if (context_) {
        context_->Release();
        context_ = nullptr;
    }
    if (engine_) {
        engine_->ShutDownAndRelease();
        engine_ = nullptr;
    }
}

void EditorToolManager::BeginFrame(SceneContext *sceneContext) {
    currentSceneContext_ = sceneContext;
    EnsureLoaded();
}

void EditorToolManager::EnsureLoaded() {
    if (loaded_) return;
    loaded_ = true;
    LoadTools();
}

void EditorToolManager::LoadTools() {
    if (!IsDirectoryExist(kEditorToolsDirectory)) {
        return;
    }

    engine_ = asCreateScriptEngine();
    if (!engine_) {
        Log("EditorTool: asCreateScriptEngine に失敗しました", LogSeverity::Error);
        return;
    }
    // ImGuiバインディングのCheckbox/DragFloat/InputText等は「現在値を渡して書き換えてもらう」ために
    // 値型（bool/int/float/string/Vector3等）の&inout参照を使う。AngelScriptの既定ではオブジェクト
    // ハンドル非対応の型へ&inoutを使えない（登録がasINVALID_DECLARATIONで失敗する）ため、
    // エディターツール専用のこのエンジンに限り参照の安全性チェックを緩和する
    // （ゲームプレイ用のSceneScriptEngineには影響しない）
    engine_->SetEngineProperty(asEP_ALLOW_UNSAFE_REFERENCES, 1);
    engine_->SetMessageCallback(asFUNCTION(EditorToolMessageCallback), nullptr, asCALL_CDECL);
    RegisterScriptArray(engine_, true);
    RegisterStdString(engine_);
    RegisterScriptDictionary(engine_);
    RegisterExceptionRoutines(engine_);
    RegisterEngineScriptBindings(engine_);
    // エディターツール専用: ImGuiの関数群（ImGui::Text等）を登録する
    // （ゲームプレイ用のSceneScriptEngineには登録されないため、ScriptComponentからは使えない）
    RegisterImGuiScriptBindings(engine_);

    // VSCodeのAngelScript Language Server用の型定義ファイルをEditorToolsフォルダ内に生成する
    // （ゲームプレイ用の as.predefined とは登録内容が異なる＝ImGui::を含むため別ファイルにする）
    if (GenerateScriptPredefinedFile(engine_, std::string(kEditorToolsDirectory) + "/as.predefined")) {
        Log("EditorTool: EditorTools/as.predefined を生成しました");
    } else {
        Log("EditorTool: EditorTools/as.predefined の生成に失敗しました", LogSeverity::Warning);
    }

    context_ = engine_->CreateContext();
    if (!context_) {
        Log("EditorTool: コンテキストの生成に失敗しました", LogSeverity::Error);
        return;
    }

    // EditorToolsフォルダ以下の.asを再帰的に列挙する
    std::vector<std::string> scriptPaths;
    const auto dir = GetDirectoryDataByExtension(kEditorToolsDirectory, { ".as" }, true, true);
    std::function<void(const DirectoryData &)> flatten = [&](const DirectoryData &d) {
        for (const auto &file : d.files) scriptPaths.push_back(file);
        for (const auto &subdir : d.subdirectories) flatten(subdir);
    };
    flatten(dir);

    asITypeInfo *interfaceType = engine_->GetTypeInfoByDecl(kEditorToolInterfaceName);
    if (!interfaceType) {
        Log("EditorTool: EditorToolインターフェースが登録されていません", LogSeverity::Error);
        return;
    }

    size_t moduleIndex = 0;
    for (const auto &scriptPath : scriptPaths) {
        const std::string moduleName = "EditorTool_" + std::to_string(moduleIndex++);

        CScriptBuilder builder;
        if (builder.StartNewModule(engine_, moduleName.c_str()) < 0) {
            Log("EditorTool: モジュールの作成に失敗しました: " + scriptPath, LogSeverity::Error);
            continue;
        }
        builder.SetIncludeCallback(ResolveEditorToolIncludePath, nullptr);
        if (builder.AddSectionFromFile(scriptPath.c_str()) < 0 || builder.BuildModule() < 0) {
            Log("EditorTool: スクリプトのビルドに失敗しました: " + scriptPath, LogSeverity::Error);
            engine_->DiscardModule(moduleName.c_str());
            continue;
        }

        asIScriptModule *module = builder.GetModule();
        if (!module) continue;

        // EditorToolを実装した全クラスを探してインスタンス化する
        const asUINT typeCount = module->GetObjectTypeCount();
        for (asUINT typeIndex = 0; typeIndex < typeCount; ++typeIndex) {
            asITypeInfo *type = module->GetObjectTypeByIndex(typeIndex);
            if (!type || !type->Implements(interfaceType)) continue;

            const std::string factoryDecl = std::string(type->GetName()) + " @" + type->GetName() + "()";
            asIScriptFunction *factory = type->GetFactoryByDecl(factoryDecl.c_str());
            if (!factory) {
                Log(std::string("EditorTool: クラス ") + type->GetName() + " のデフォルトコンストラクタが見つかりません", LogSeverity::Error);
                continue;
            }
            if (context_->Prepare(factory) < 0) continue;
            int r;
            {
                ScriptExecutionScope scope(nullptr, currentSceneContext_);
                r = context_->Execute();
            }
            if (r != asEXECUTION_FINISHED) {
                Log("EditorTool: " + std::string(type->GetName()) + " のインスタンス生成に失敗しました: " + GetExceptionInfo(context_), LogSeverity::Error);
                continue;
            }
            asIScriptObject *object = *static_cast<asIScriptObject **>(context_->GetAddressOfReturnValue());
            if (!object) continue;
            object->AddRef();

            ToolInstance tool;
            tool.object = object;
            tool.type = type;
            tool.initializeOnLoadMethod = type->GetMethodByDecl("void InitializeOnLoad()");
            tool.onItemSelectedMethod = type->GetMethodByDecl("void OnItemSelected(const string &in)");
            tool.onWindowEnableMethod = type->GetMethodByDecl("void OnWindowEnable(const string &in)");
            tool.onWindowDisableMethod = type->GetMethodByDecl("void OnWindowDisable(const string &in)");
            tool.updateMethod = type->GetMethodByDecl("void Update()");

            const size_t toolIndex = tools_.size();

            // クラス属性（[EditorWindow]/[MenuItem]）を解析する
            const auto metadataList = builder.GetMetadataForType(type->GetTypeId());
            for (const auto &attribute : ParseToolAttributeTokens(metadataList)) {
                if (attribute.name == "EditorWindow") {
                    if (attribute.args.empty() || attribute.args[0].empty()) {
                        Log(std::string("EditorTool: ") + type->GetName() + " の [EditorWindow] にウィンドウ名がありません", LogSeverity::Warning);
                        continue;
                    }
                    WindowEntry window;
                    window.name = attribute.args[0];
                    window.toolIndex = toolIndex;
                    tool.windowIndices.push_back(windows_.size());
                    windows_.push_back(std::move(window));
                } else if (attribute.name == "MenuItem") {
                    if (attribute.args.empty() || attribute.args[0].empty()) {
                        Log(std::string("EditorTool: ") + type->GetName() + " の [MenuItem] に表示階層がありません", LogSeverity::Warning);
                        continue;
                    }
                    // 第二引数（識別タグ）が省略された場合は項目名をタグとして使う
                    const std::string tag = attribute.args.size() >= 2 ? attribute.args[1] : std::string{};
                    RegisterMenuItem(attribute.args[0], tag, toolIndex);
                }
            }

            tools_.push_back(std::move(tool));
            Log(std::string("EditorTool: ") + type->GetName() + " を読み込みました (" + scriptPath + ")");
        }
    }

    // 全ツールの読み込みが終わってから、各ツールのInitializeOnLoadを一度だけ呼ぶ
    for (auto &tool : tools_) {
        CallToolMethod(tool, tool.initializeOnLoadMethod, nullptr);
    }
}

void EditorToolManager::RegisterMenuItem(const std::string &path, const std::string &tag, size_t toolIndex) {
    auto segments = SplitMenuPath(path);
    if (segments.size() < 2) {
        Log("EditorTool: [MenuItem] の表示階層は \"MenuBar/項目名\" または \"Hierarchy/項目名\" の形式で指定してください: " + path, LogSeverity::Warning);
        return;
    }

    MenuNode *root = nullptr;
    if (segments[0] == "MenuBar") {
        root = &menuBarRoot_;
    } else if (segments[0] == "Hierarchy") {
        root = &hierarchyRoot_;
    } else {
        Log("EditorTool: [MenuItem] の表示階層の先頭は \"MenuBar/\" または \"Hierarchy/\" のみ対応しています: " + path, LogSeverity::Warning);
        return;
    }

    MenuNode *node = root;
    for (size_t i = 1; i + 1 < segments.size(); ++i) {
        node = &node->children[segments[i]];
    }

    MenuItemEntry entry;
    entry.label = segments.back();
    entry.tag = tag.empty() ? segments.back() : tag;
    entry.toolIndex = toolIndex;
    node->items.push_back(std::move(entry));
}

void EditorToolManager::ShowMenuNode(const MenuNode &node) {
    for (const auto &[name, child] : node.children) {
        if (ImGui::BeginMenu(name.c_str())) {
            ShowMenuNode(child);
            ImGui::EndMenu();
        }
    }
    for (const auto &item : node.items) {
        if (ImGui::MenuItem(item.label.c_str())) {
            if (item.toolIndex < tools_.size()) {
                auto &tool = tools_[item.toolIndex];
                CallToolMethod(tool, tool.onItemSelectedMethod, &item.tag);
            }
        }
    }
}

void EditorToolManager::ShowMenuBarItems() {
    EnsureLoaded();
    ShowMenuNode(menuBarRoot_);
}

void EditorToolManager::ShowHierarchyMenuItems() {
    EnsureLoaded();
    ShowMenuNode(hierarchyRoot_);
}

void EditorToolManager::UpdateTools() {
    EnsureLoaded();

    // ウィンドウの開閉遷移を通知する（開いた: OnWindowEnable、閉じた: OnWindowDisable）
    for (auto &window : windows_) {
        if (window.isOpen != window.wasOpen) {
            if (window.toolIndex < tools_.size()) {
                auto &tool = tools_[window.toolIndex];
                CallToolMethod(tool, window.isOpen ? tool.onWindowEnableMethod : tool.onWindowDisableMethod, &window.name);
            }
            window.wasOpen = window.isOpen;
        }
    }

    // 各ツールのUpdateを呼ぶ。開いているウィンドウを持つツールは、そのウィンドウのBegin/Endの中で呼ぶ
    // （閉じるボタン(X)でisOpenがfalseへ変わった場合は、次フレームの上の遷移通知でOnWindowDisableが走る）
    for (auto &tool : tools_) {
        WindowEntry *openWindow = nullptr;
        for (const size_t windowIndex : tool.windowIndices) {
            if (windowIndex < windows_.size() && windows_[windowIndex].isOpen) {
                openWindow = &windows_[windowIndex];
                break;
            }
        }
        if (openWindow) {
            ImGui::Begin(openWindow->name.c_str(), &openWindow->isOpen);
            CallToolMethod(tool, tool.updateMethod, nullptr);
            ImGui::End();
        } else {
            CallToolMethod(tool, tool.updateMethod, nullptr);
        }
    }
}

void EditorToolManager::OpenWindow(const std::string &name) {
    for (auto &window : windows_) {
        if (window.name == name) window.isOpen = true;
    }
}

void EditorToolManager::CloseWindow(const std::string &name) {
    for (auto &window : windows_) {
        if (window.name == name) window.isOpen = false;
    }
}

bool EditorToolManager::IsWindowOpen(const std::string &name) const {
    for (const auto &window : windows_) {
        if (window.name == name && window.isOpen) return true;
    }
    return false;
}

void EditorToolManager::CallToolMethod(ToolInstance &tool, asIScriptFunction *method, const std::string *stringArg) {
    if (!method || !context_ || !tool.object) return;

    if (context_->Prepare(method) < 0) return;
    context_->SetObject(tool.object);
    if (stringArg) {
        context_->SetArgObject(0, const_cast<std::string *>(stringArg));
    }
    ScriptExecutionScope scope(nullptr, currentSceneContext_);
    const int r = context_->Execute();
    if (r != asEXECUTION_FINISHED) {
        Log("EditorTool: " + GetExceptionInfo(context_), LogSeverity::Error);
    }
}

} // namespace KashipanEngine
#endif // USE_IMGUI
