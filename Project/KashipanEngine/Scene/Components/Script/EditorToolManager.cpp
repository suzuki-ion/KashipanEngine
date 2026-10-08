#include "Scene/Components/Script/EditorToolManager.h"
#ifdef USE_IMGUI

#include <functional>
#include <cstring>
#include <algorithm>
#include <filesystem>
#include <memory>
#include <unordered_set>

#include <angelscript.h>
#include <add_on/scriptarray/scriptarray.h>
#include <add_on/scriptbuilder/scriptbuilder.h>
#include <add_on/scriptdictionary/scriptdictionary.h>
#include <add_on/scripthelper/scripthelper.h>
#include <add_on/scriptstdstring/scriptstdstring.h>

#include <imgui.h>
#include <imgui_internal.h>
#include "Utilities/Conversion/ConvertString.h"

#include "Debug/Logger.h"
#include "Scene/Components/Script/AngelScriptDebugServer.h"
#include "Scene/Components/Script/ImGuiScriptBindings.h"
#include "Core/ProjectPaths.h"
#include "Scene/Components/Script/ScriptBindings.h"
#include "Utilities/FileIO/Directory.h"

namespace KashipanEngine {

namespace {

/// @brief エディターツールのフォルダ名（エンジンルート直下）
/// @details ゲーム固有のデータではなくエンジン共通のエディター拡張のため、
///          プロジェクトではなくエンジンルート基準で探す
constexpr const char *kEditorToolsDirectory = "EditorTools";
constexpr const char *kEditorToolInterfaceName = "EditorTool";

class ImGuiScriptRecovery final {
public:
    ImGuiScriptRecovery() {
        context_ = ImGui::GetCurrentContext();
        if (context_ && context_->WithinFrameScope && context_->CurrentWindow) {
            ImGui::ErrorRecoveryStoreState(&state_);
            previousBoundary_ = SetImGuiScriptBoundary(&state_);
        }
        else context_ = nullptr;
    }
    ~ImGuiScriptRecovery() { Recover(); }
    bool Recover() {
        if (!context_) return false;
        ImGuiErrorRecoveryState current;
        ImGui::ErrorRecoveryStoreState(&current);
        const bool changed = std::memcmp(&state_, &current, sizeof(state_)) != 0;
        const bool enableAssert = context_->IO.ConfigErrorRecoveryEnableAssert;
        context_->IO.ConfigErrorRecoveryEnableAssert = false;
        ImGui::ErrorRecoveryTryToRecoverState(&state_);
        context_->IO.ConfigErrorRecoveryEnableAssert = enableAssert;
        SetImGuiScriptBoundary(previousBoundary_);
        context_ = nullptr;
        return changed;
    }
private:
    ImGuiContext *context_ = nullptr;
    ImGuiErrorRecoveryState state_;
    const ImGuiErrorRecoveryState *previousBoundary_ = nullptr;
};


void EditorToolMessageCallback(const asSMessageInfo *msg, void *param) {
    if (msg->type == asMSGTYPE_ERROR && param) {
        *static_cast<std::string *>(param) += std::string(msg->section) + "(" + std::to_string(msg->row) + "): " + msg->message + "\n";
    }
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

EditorToolManager *EditorToolManager::loadingInstance_ = nullptr;

EditorToolManager &EditorToolManager::GetInstance() {
    if (loadingInstance_) return *loadingInstance_;
    static EditorToolManager instance;
    return instance;
}

EditorToolManager::~EditorToolManager() { ReleaseTools(); }

void EditorToolManager::ReleaseTools() {
    struct RestoreInstance {
        EditorToolManager *previous;
        ~RestoreInstance() { loadingInstance_ = previous; }
    } restore{loadingInstance_};
    loadingInstance_ = this;
    ImGuiScriptRecovery recovery;
    ScriptExecutionScope scope(nullptr, nullptr);
    if (context_) {
        context_->ClearLineCallback();
        context_->Unprepare();
    }
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
    if (reloadRequested_) { reloadRequested_ = false; ReloadTools(); }
}

void EditorToolManager::EnsureLoaded() {
    if (loaded_) return;
    loaded_ = true;
    ReloadTools();
}

bool EditorToolManager::ReloadTools() {
    EditorToolManager candidate;
    candidate.currentSceneContext_ = currentSceneContext_;
    struct LoadingScope {
        EditorToolManager *previous;
        LoadingScope(EditorToolManager *manager) : previous(loadingInstance_) { loadingInstance_ = manager; }
        ~LoadingScope() { loadingInstance_ = previous; }
    };
    bool success;
    { LoadingScope scope(&candidate); success = candidate.LoadTools(); }
    if (!success) {
        lastError_ = candidate.lastError_.empty() ? "Editor tool reload failed. Existing tools were retained." : candidate.lastError_;
        Log(lastError_, LogSeverity::Error);
        return false;
    }
    std::swap(engine_, candidate.engine_);
    std::swap(context_, candidate.context_);
    tools_.swap(candidate.tools_);
    windows_.swap(candidate.windows_);
    std::swap(menuBarRoot_, candidate.menuBarRoot_);
    std::swap(hierarchyRoot_, candidate.hierarchyRoot_);
    if (engine_) engine_->SetMessageCallback(asFUNCTION(EditorToolMessageCallback), &lastError_, asCALL_CDECL);
    if (candidate.engine_) candidate.engine_->SetMessageCallback(asFUNCTION(EditorToolMessageCallback), &candidate.lastError_, asCALL_CDECL);
    lastError_.clear();
    Log("Editor tools reloaded; tool state was reset.");
    return true;
}

bool EditorToolManager::LoadTools() {
    const std::string editorToolsDirectory = ProjectPaths::InEngineRoot(kEditorToolsDirectory);
    if (!IsDirectoryExist(editorToolsDirectory)) return true;

    engine_ = asCreateScriptEngine();
    if (!engine_) {
        Log(Translation("engine.editortool.failed.createengine"), LogSeverity::Error);
        return false;
    }
    // ImGuiバインディングのCheckbox/DragFloat/InputText等は「現在値を渡して書き換えてもらう」ために
    // 値型（bool/int/float/string/Vector3等）の&inout参照を使う。AngelScriptの既定ではオブジェクト
    // ハンドル非対応の型へ&inoutを使えない（登録がasINVALID_DECLARATIONで失敗する）ため、
    // エディターツール専用のこのエンジンに限り参照の安全性チェックを緩和する
    // （ゲームプレイ用のSceneScriptEngineには影響しない）
    engine_->SetEngineProperty(asEP_ALLOW_UNSAFE_REFERENCES, 1);
    engine_->SetMessageCallback(asFUNCTION(EditorToolMessageCallback), &lastError_, asCALL_CDECL);
    engine_->SetEngineProperty(asEP_INIT_GLOBAL_VARS_AFTER_BUILD, 0);
    RegisterScriptArray(engine_, true);
    RegisterStdString(engine_);
    RegisterScriptDictionary(engine_);
    RegisterExceptionRoutines(engine_);
    RegisterEngineScriptBindings(engine_);
    // エディターツール専用: ImGuiの関数群（ImGui::Text等）を登録する
    // （ゲームプレイ用のSceneScriptEngineには登録されないため、ScriptComponentからは使えない）
    RegisterImGuiScriptBindings(engine_);

    context_ = engine_->CreateContext();
    if (!context_) {
        Log(Translation("engine.editortool.failed.createcontext"), LogSeverity::Error);
        return false;
    }
#if !defined(RELEASE_BUILD)
    auto &debugServer = GetProcessAngelScriptDebugServer();
    if (debugServer.Start()) {
        debugServer.AttachContext(context_);
    }
#endif

    // EditorToolsフォルダ以下の.asを再帰的に列挙する
    std::vector<std::string> scriptPaths;
    const auto dir = GetDirectoryDataByExtension(editorToolsDirectory, { ".as" }, true, true);
    std::function<void(const DirectoryData &)> flatten = [&](const DirectoryData &d) {
        for (const auto &file : d.files) scriptPaths.push_back(file);
        for (const auto &subdir : d.subdirectories) flatten(subdir);
    };
    flatten(dir);
    std::sort(scriptPaths.begin(), scriptPaths.end());
    std::unordered_set<std::string> registeredTypes;

    asITypeInfo *interfaceType = engine_->GetTypeInfoByDecl(kEditorToolInterfaceName);
    if (!interfaceType) {
        Log(Translation("engine.editortool.interface.notregistered"), LogSeverity::Error);
        return false;
    }

    std::vector<asIScriptFunction *> factories;
    size_t moduleIndex = 0;
    for (const auto &scriptPath : scriptPaths) {
        const std::string moduleName = "EditorTool_" + std::to_string(moduleIndex++);

        CScriptBuilder builder;
        if (builder.StartNewModule(engine_, moduleName.c_str()) < 0) {
            Log(Translation("engine.editortool.failed.createmodule") + scriptPath, LogSeverity::Error);
            return false;
        }
        builder.SetIncludeCallback(ResolveEditorToolIncludePath, nullptr);
        if (builder.AddSectionFromFile(scriptPath.c_str()) < 0 || builder.BuildModule() < 0) {
            Log(Translation("engine.editortool.failed.build") + scriptPath, LogSeverity::Error);
            engine_->DiscardModule(moduleName.c_str());
            return false;
        }

        asIScriptModule *module = builder.GetModule();
        if (!module) return false;

        // EditorToolを実装した全クラスを探してインスタンス化する
        const asUINT typeCount = module->GetObjectTypeCount();
        for (asUINT typeIndex = 0; typeIndex < typeCount; ++typeIndex) {
            asITypeInfo *type = module->GetObjectTypeByIndex(typeIndex);
            if (!type || !type->Implements(interfaceType)) continue;

            asIScriptFunction *factory = nullptr;
            for (asUINT i = 0; i < type->GetFactoryCount(); ++i) {
                auto *entry = type->GetFactoryByIndex(i);
                if (entry && entry->GetParamCount() == 0) { factory = entry; break; }
            }
            if (!factory) {
                Log(Translation("engine.editortool.defaultconstructor.notfound") + type->GetName(), LogSeverity::Error);
                return false;
            }
            const char *declaredSection = nullptr;
            factory->GetDeclaredAt(&declaredSection, nullptr, nullptr);
            for (asUINT i = 0; !declaredSection && i < type->GetBehaviourCount(); ++i) {
                asEBehaviours behaviour;
                auto *method = type->GetBehaviourByIndex(i, &behaviour);
                if (behaviour == asBEHAVE_CONSTRUCT && method) method->GetDeclaredAt(&declaredSection, nullptr, nullptr);
            }
            const std::string section = declaredSection ? declaredSection : "";
            if (section.empty()) { lastError_ = "Cannot identify editor tool source: " + std::string(type->GetName()); return false; }
            std::error_code pathError;
            const auto sourcePath = std::filesystem::absolute(Utf8StringToPath(section), pathError).lexically_normal();
            if (pathError) { lastError_ = "Cannot resolve editor tool source: " + section; return false; }
            const std::string identity = PathToUtf8String(sourcePath) + "::" + type->GetNamespace() + "::" + type->GetName();
            if (!registeredTypes.insert(identity).second) continue;
            ToolInstance tool;
            tool.object = nullptr;
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
                        Log(Translation("engine.editortool.editorwindow.noname") + type->GetName(), LogSeverity::Warning);
                        return false;
                    }
                    if (std::any_of(windows_.begin(), windows_.end(), [&](const auto &w) { return w.name == attribute.args[0]; })) {
                        lastError_ = "Duplicate editor window name: " + attribute.args[0]; return false;
                    }
                    WindowEntry window;
                    window.name = attribute.args[0];
                    window.toolIndex = toolIndex;
                    tool.windowIndices.push_back(windows_.size());
                    windows_.push_back(std::move(window));
                } else if (attribute.name == "MenuItem") {
                    if (attribute.args.empty() || attribute.args[0].empty()) {
                        Log(Translation("engine.editortool.menuitem.nopath") + type->GetName(), LogSeverity::Warning);
                        return false;
                    }
                    // 第二引数（識別タグ）が省略された場合は項目名をタグとして使う
                    const std::string tag = attribute.args.size() >= 2 ? attribute.args[1] : std::string{};
                    RegisterMenuItem(attribute.args[0], tag, toolIndex);
                }
            }

            factories.push_back(factory);
            tools_.push_back(std::move(tool));
            Log(Translation("engine.editortool.loaded") + type->GetName() + " (" + scriptPath + ")");
        }
    }

    // Compilation succeeds for the entire candidate before executing any script.
    ScriptExecutionScope executionScope(nullptr, currentSceneContext_);
    for (asUINT i = 0; i < engine_->GetModuleCount(); ++i) {
        ImGuiScriptRecovery recovery;
        if (engine_->GetModuleByIndex(i)->ResetGlobalVars(context_) < 0) {
            lastError_ = "Editor tool global initialization failed: " + GetExceptionInfo(context_);
            return false;
        }
        if (recovery.Recover()) { lastError_ = "Unbalanced ImGui state in global initialization"; return false; }
    }
    for (size_t i = 0; i < tools_.size(); ++i) {
        ImGuiScriptRecovery recovery;
        if (context_->Prepare(factories[i]) < 0 || context_->Execute() != asEXECUTION_FINISHED) {
            lastError_ = "Editor tool constructor failed: " + GetExceptionInfo(context_);
            return false;
        }
        auto *object = *static_cast<asIScriptObject **>(context_->GetAddressOfReturnValue());
        if (!object) return false;
        object->AddRef();
        tools_[i].object = object;
        if (recovery.Recover()) { lastError_ = "Unbalanced ImGui state in editor tool constructor"; return false; }
    }
    for (auto &tool : tools_) {
        if (!CallToolMethod(tool, tool.initializeOnLoadMethod, nullptr)) return false;
    }
    context_->Unprepare();
    GenerateScriptPredefinedFile(engine_, editorToolsDirectory + "/as.predefined");
    return true;
}

void EditorToolManager::RegisterMenuItem(const std::string &path, const std::string &tag, size_t toolIndex) {
    auto segments = SplitMenuPath(path);
    if (segments.size() < 2) {
        Log(Translation("engine.editortool.menuitem.invalidpath") + path, LogSeverity::Warning);
        return;
    }

    MenuNode *root = nullptr;
    if (segments[0] == "MenuBar") {
        root = &menuBarRoot_;
    } else if (segments[0] == "Hierarchy") {
        root = &hierarchyRoot_;
    } else {
        Log(Translation("engine.editortool.menuitem.invalidroot") + path, LogSeverity::Warning);
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
    if (ImGui::BeginMenu(TranslationLabel("editor.editortools.menu"))) {
        if (ImGui::MenuItem(TranslationLabel("editor.editortools.reload"))) RequestReload();
        if (!lastError_.empty()) ImGui::TextWrapped("%s", lastError_.c_str());
        ImGui::EndMenu();
    }
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
            window.wasOpen = !window.wasOpen;
        }
    }

    for (auto &tool : tools_) {
        if (tool.failed) continue;
        if (tool.windowIndices.empty()) {
            CallToolMethod(tool, tool.updateMethod, nullptr);
            continue;
        }
        // Each open declared window is drawn; closed window tools do not update.
        for (const size_t index : tool.windowIndices) {
            auto &window = windows_[index];
            if (!window.isOpen || tool.failed) continue;
            currentWindowName_ = window.name;
            const bool visible = ImGui::Begin(window.name.c_str(), &window.isOpen);
            if (visible && window.isOpen) CallToolMethod(tool, tool.updateMethod, nullptr);
            ImGui::End();
            currentWindowName_.clear();
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

bool EditorToolManager::CallToolMethod(ToolInstance &tool, asIScriptFunction *method, const std::string *stringArg) {
    if (tool.failed) return false;
    if (!method) return true;
    if (!context_ || !tool.object) return false;
    if (context_->Prepare(method) < 0) { tool.failed = true; lastError_ = "Cannot prepare editor tool method"; return false; }
    context_->SetObject(tool.object);
    if (stringArg) context_->SetArgObject(0, const_cast<std::string *>(stringArg));
    ImGuiScriptRecovery recovery;
    ScriptExecutionScope scope(nullptr, currentSceneContext_);
    const int result = context_->Execute();
    const bool unbalanced = recovery.Recover();
    if (result != asEXECUTION_FINISHED || unbalanced) {
        tool.failed = true;
        lastError_ = std::string(tool.type->GetName()) + "::" + method->GetName() + ": " + (unbalanced ? "Unbalanced ImGui state" : GetExceptionInfo(context_));
        Log(lastError_, LogSeverity::Error);
        return false;
    }
    return true;
}

} // namespace KashipanEngine
#endif // USE_IMGUI
