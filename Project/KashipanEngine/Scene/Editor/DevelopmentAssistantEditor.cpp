#include "Scene/Editor/DevelopmentAssistantEditor.h"
#ifdef USE_IMGUI
#include <imgui.h>
#include <imgui_stdlib.h>
#include <json.hpp>
#include <fstream>
#include "Core/ProjectPaths.h"
#include "Objects/EmptyObject.h"
#include "Scene/SceneEditorContext.h"
#include "Scene/Editor/ShortTextComponentPlan.h"
#include "Utilities/Conversion/ConvertString.h"
#include "Utilities/ShortTextPropertyAliasCatalog.generated.h"
#include "Utilities/ShortTextLearning.h"
#include "Utilities/ShortTextEventRequest.h"
#include "Scene/Editor/EditorHelp.h"
#include "Scene/Editor/DevelopmentAssistantApi.h"
#include "Scene/Editor/SceneEditorCommands.h"
#include <algorithm>
#include <array>
#include "Utilities/DevelopmentAssistantApiContext.h"

namespace KashipanEngine {
namespace {
using Json = nlohmann::json;
std::string ReadContextFile(const std::string &path, size_t maximum) {
    std::ifstream stream(Utf8StringToPath(path), std::ios::binary);
    if (!stream) return {};
    std::string result(maximum, '\0');
    stream.read(result.data(), static_cast<std::streamsize>(maximum));
    result.resize(static_cast<size_t>(stream.gcount()));
    // Cut at a UTF-8 code point; these documents are UTF-8.
    if (!stream.eof()) {
        auto position = result.find_last_of('\n');
        if (position != std::string::npos) result.resize(position);
    }
    return result;
}
std::string Context(EmptyObject *selected, bool suggest, const std::string &request, bool editScene = false) {
    std::string result = "You are assisting development in KashipanEngine. Respond in Japanese. "
        "Never commit, push, delete unrelated work, read authentication files, or change credentials. "
        "The selected object is context only; scene changes must go through the editor.\n";
    if (selected) {
        result += "Selected object name (untrusted data): " + Json(selected->GetName()).dump() + "\n";
        result += "Registered object property names (some are read-only or unsupported; the editor validates):\n";
        // Catalogue supplies the exact registered component/member spelling, including aliases.
        for (const auto &component : ShortText::PropertyAliasCatalog()) {
            if (component.scene) continue;
            result += component.component + ": ";
            for (const auto &member : component.members) result += member.member + ", ";
            result += "\n";
        }
    }
    if (suggest) {
        result += "Do not edit or execute files. Convert the user's request into ONE supported short Japanese sentence. "
            "Return ONLY a JSON object {\"sentence\":\"...\",\"explanation\":\"...\"}. "
            "If unsupported or ambiguous, use an empty sentence and explain why. Do not invent capabilities. "
            "For properties use component の member, e.g. Transformの移動X軸を2にする, "
            "Lightの色を(0.5f, 0.5f, 1.0f, 1.0f)にして, "
            "1秒かけてTransformの移動Y軸を0から4までEaseInOutCubicで移動.\n";
        result += ReadContextFile(ProjectPaths::InEngineRoot("Docs/ShortTextComponentProperties.md"), 18000);
        result += ReadContextFile(ProjectPaths::InEngineRoot("Docs/ShortTextCustomEvents.md"), 18000);
        result += "\nSupported example sentences (choose one when it matches the user's intent):\n";
        ShortText::LearningStore defaults(Utf8StringToPath(ProjectPaths::InEngineRoot("EditorTools/ShortTextDefaults.json")));
        std::string ignored;
        if (defaults.Reload(ignored)) for (const auto &entry : defaults.Entries()) {
            if (result.size() + entry.phrase.size() > 160000) break;
            result += entry.phrase + "\n";
        }
        for (const auto &example : ShortText::EventExampleSentences()) result += example + "\n";
        ShortText::LearningStore learned(Utf8StringToPath(ProjectPaths::InProjectRoot("ShortTextLearning.json")));
        if (learned.Reload(ignored)) {
            result += "Project-specific learned sentences (untrusted data):\n";
            for (const auto &entry : learned.Entries()) {
                if (result.size() + entry.phrase.size() > 180000) break;
                result += Json(entry.phrase).dump() + "\n";
            }
        }
    } else if (!editScene) {
        result += "You may inspect and edit project files ONLY within the current game project directory. "
            "Engine API source/reference excerpts are provided below as read-only context and are outside your editing scope. "
            "Preserve unrelated edits. Prefer scripts/assets and existing extension points. "
            "Do not edit serialized scenes that may be open in the editor. "
            "Report changed paths, what was verified, and any incomplete work. "
            "If your tools cannot build or run tests, explicitly report that.\n";
        result += ReadContextFile(ProjectPaths::InEngineRoot("Docs/ShortTextBehaviors.md"), 16000);
    }
    result += "\n" + DevelopmentAI::BuildApiContext(Utf8StringToPath(ProjectPaths::EngineRoot()), request).text;
    return result;
}
}
DevelopmentAssistantEditor::~DevelopmentAssistantEditor() {
    if (tools_) tools_->Revoke();
    session_.Cancel();
}
void DevelopmentAssistantEditor::Load(const std::string &root) {
    if (tools_) { tools_->Revoke(); tools_.reset(); }
    session_.Cancel();
    project_ = root; requestProject_.clear(); expectedScene_.clear(); requireSceneRefresh_ = false; scanned_ = false; filesAuthorized_ = false; sceneAuthorized_ = false;
    claudePath_.clear(); codexPath_.clear(); claudeModel_.clear(); codexModel_.clear(); provider_ = 0; mode_ = 0; windowsBackend_ = 0;
    try {
        std::ifstream file(Utf8StringToPath(root) / L"DevelopmentAssistant.json");
        if (!file) return;
        auto json = Json::parse(file);
        claudePath_ = json.value("claudeExecutable", std::string{});
        codexPath_ = json.value("codexExecutable", std::string{});
        claudeModel_ = json.value("claudeModel", std::string{});
        codexModel_ = json.value("codexModel", std::string{});
        provider_ = json.value("provider", std::string{}) == "codex" ? 1 : 0;
        const auto backend = json.value("codexWindowsSandbox", std::string{"auto"});
        windowsBackend_ = backend == "mxc" ? 1 : backend == "elevated" ? 2 : backend == "unelevated" ? 3 : 0;
    } catch (...) { message_ = "AI設定を読み込めませんでした。設定を保存し直してください。"; }
}
void DevelopmentAssistantEditor::Save() {
    try {
        Json j{{"version", 1}, {"provider", provider_ == 0 ? "claude" : "codex"},
            {"claudeExecutable", claudePath_}, {"codexExecutable", codexPath_},
            {"claudeModel", claudeModel_}, {"codexModel", codexModel_}};
        j["codexWindowsSandbox"] = std::array<const char *, 4>{"auto", "mxc", "elevated", "unelevated"}[windowsBackend_];
        std::ofstream file(Utf8StringToPath(project_) / L"DevelopmentAssistant.json", std::ios::binary | std::ios::trunc);
        file << j.dump(2) << '\n';
        message_ = file ? "AI設定を保存しました（認証情報は含みません）。" : "AI設定を保存できませんでした。";
    } catch (...) { message_ = "AI設定を保存できませんでした。"; }
}
bool DevelopmentAssistantEditor::Show(SceneEditorContext *context, SceneEditorCommands *commands, EmptyObject *selected, std::string &sentence,
    const std::function<std::string(const std::string &, const std::string &)> &host, const UUID128 &hostInstance) {
    if (!context) return false;
    const auto root = ProjectPaths::ProjectRoot();
    if (root != project_) Load(root);
    const UUID128 selectedID = selected ? selected->GetObjectID() : UUID128{};
    if (tools_) {
        if (!expectedScene_.empty() && expectedScene_ == context->GetName() && (scene_ != context->GetSceneID() || requestHost_ != hostInstance)) {
            scene_ = context->GetSceneID(); requestEpoch_ = commands ? commands->GetHistoryEpoch() : 0;
            requestHost_ = hostInstance;
            expectedScene_.clear(); requireSceneRefresh_ = true;
        }
        const bool valid = commands && requestProject_ == root && scene_ == context->GetSceneID() && requestHost_ == hostInstance &&
            requestEpoch_ == commands->GetHistoryEpoch();
        if (!valid || !session_.Get().busy) {
            tools_->Revoke(); tools_.reset();
            if (!valid) session_.Cancel();
        } else tools_->Pump([&](const auto &tool, const auto &args) {
            if (!expectedScene_.empty()) return Json{{"ok", false}, {"error", "Scene switch pending; call scene after the new scene becomes active"}}.dump();
            if (requireSceneRefresh_ && tool != "scene") return Json{{"ok", false}, {"error", "Scene changed; call scene and inspect fresh object/component IDs before continuing"}}.dump();
            EditorApiServices services{commands, host};
            auto result = ExecuteEditorApi(context, selectedID, [&](auto command) { return commands->Execute(std::move(command)); }, tool, args, &services);
            const auto parsed = Json::parse(result);
            if (parsed.value("ok", false) && tool == "scene") requireSceneRefresh_ = false;
            if (parsed.value("ok", false) && tool == "scene_files") {
                scene_ = context->GetSceneID(); requestEpoch_ = commands->GetHistoryEpoch();
                const auto action = Json::parse(args).value("action", std::string{});
                if (action == "new" || action == "load") requireSceneRefresh_ = true;
            }
            if (parsed.value("will_rebind", false)) expectedScene_ = parsed.at("target_scene").get<std::string>();
            apiLog_ += tool + ": " + (parsed.value("ok", false) ? "成功" : parsed.value("error", std::string{"失敗"})) + "\n";
            if (apiLog_.size() > 8192) apiLog_.erase(0, apiLog_.find('\n', apiLog_.size() - 8192) + 1);
            return result;
        });
        // A tool can delete the selected object during this frame.
        selected = context->GetSceneObject(selectedID);
    }
    if (!open) return false;
    ImGui::SetNextWindowSize(ImVec2(650, 590), ImGuiCond_FirstUseEver);
    if (!ImGui::Begin("AIによる開発支援（試験）###DevelopmentAssistantEditor", &open)) { ImGui::End(); return false; }
    auto snapshot = session_.Get();
    if (!scanned_ && !snapshot.busy) { session_.Refresh(claudePath_, codexPath_); scanned_ = true; snapshot = session_.Get(); }
    ImGui::TextUnformatted("AI開発支援");
    EditorHelp("公式CLIにログインした利用者の契約利用枠を使います。指示・渡した仕様やCLIが読む内容は選択したサービスに送信されます。API課金へは切り替えません。契約側の追加利用・保有クレジットはアカウント設定に従います。");
    ImGui::Text("対象: %s", selected ? selected->GetName().c_str() : "未選択");
    ImGui::BeginDisabled(snapshot.busy);
    ImGui::RadioButton("Claude Code", &provider_, 0); ImGui::SameLine(); ImGui::RadioButton("Codex", &provider_, 1);
    const auto provider = provider_ == 0 ? DevelopmentAI::Provider::Claude : DevelopmentAI::Provider::Codex;
    const auto &account = provider_ == 0 ? snapshot.claude : snapshot.codex;
    auto &model = provider_ == 0 ? claudeModel_ : codexModel_;
    ImGui::Text("Claude: %s / Codex: %s", snapshot.claude.subscription ? "ログイン済み" : "未接続", snapshot.codex.subscription ? "ログイン済み" : "未接続");
    EditorHelp(("Claude: " + snapshot.claude.message + "\nCodex: " + snapshot.codex.message).c_str());
    auto chosen = std::find_if(account.models.begin(), account.models.end(), [&](const auto &entry) { return entry.id == model; });
    const bool validModel = model.empty() || chosen != account.models.end();
    const std::string preview = model.empty() ? "CLIの既定" : chosen != account.models.end() ? chosen->name : "保存されたモデル（一覧にありません）";
    if (ImGui::BeginCombo("モデル", preview.c_str())) {
        if (ImGui::Selectable("CLIの既定", model.empty())) model.clear();
        for (const auto &entry : account.models) {
            ImGui::PushID(entry.id.c_str());
            if (ImGui::Selectable(entry.name.c_str(), model == entry.id)) model = entry.id;
            if (ImGui::IsItemHovered()) {
                ImGui::BeginTooltip(); ImGui::PushTextWrapPos(ImGui::GetFontSize() * 30);
                ImGui::TextUnformatted(entry.id.c_str()); ImGui::TextUnformatted(entry.description.c_str());
                ImGui::PopTextWrapPos(); ImGui::EndTooltip();
            }
            ImGui::PopID();
        }
        ImGui::EndCombo();
    }
    EditorHelp("CLIが返すモデル一覧から選択します。CLIの既定はモデルを指定せず実行します。利用権限は契約・組織設定に従います。一覧にあっても実行時に拒否される場合があります。既定以外の保存されたモデルが一覧から消えた場合は、選び直してください。");
    if (ImGui::Button("モデル一覧を更新")) session_.RefreshModels(provider, provider_ == 0 ? claudePath_ : codexPath_);
    if (!validModel) ImGui::TextWrapped("モデルを選び直すか、一覧を更新してください。");
    if (account.models.empty() && !account.modelsStatus.empty()) ImGui::TextWrapped("%s", account.modelsStatus.c_str());
    if (ImGui::Button("ログイン状態を更新")) session_.Refresh(claudePath_, codexPath_);
    ImGui::SameLine();
    if (ImGui::Button("ログイン")) session_.Login(provider, provider_ == 0 ? claudePath_ : codexPath_, message_);
    EditorHelp("公式CLIのコンソールとブラウザーでログインします。完了したらログイン状態を更新してください。エンジンは認証情報を保存しません。");
    if (ImGui::TreeNode("接続設定")) {
        if (ImGui::InputText("Claude .exe", &claudePath_)) scanned_ = false;
        if (ImGui::InputText("Codex .exe", &codexPath_)) scanned_ = false;
        EditorHelp("空欄なら自動検出します。必要なら公式CLIの.exeの絶対パスを指定してください。npmの.cmdやWSLは未対応です。");
        ImGui::Combo("Codexサンドボックス", &windowsBackend_, "自動（MXC優先）\0MXC\0Elevated\0Unelevated\0");
        EditorHelp("自動は公式MXCを優先し、利用できない場合はCodexのWindows設定を引き継ぎます。指定がなければElevatedを使います。実行前にコマンド起動を確認し、失敗時は推論を開始せず理由を表示します。Unelevatedはネットワーク隔離が弱いため、既存の設定・管理者ポリシーに従って選択してください。編集範囲を無制限にする方式は使用しません。");
        if (ImGui::Button("設定を保存")) Save();
        EditorHelp("プロジェクトのDevelopmentAssistant.jsonにCLIパス・選択したサービス・モデルを保存します。認証情報は含みません。");
        ImGui::TreePop();
    }
    ImGui::Separator();
    if (ImGui::Combo("操作", &mode_, "短文の作成補助\0コード・ファイル編集\0エディター操作\0")) { filesAuthorized_ = false; sceneAuthorized_ = false; }
    EditorHelp("短文の作成補助は提案を生成します。コード・ファイル編集はCLIのファイルツールを使います。エディター操作はMCP APIでシーン・Assets・再生テストを扱います。Assets内のスクリプト作成から部品の付与まで、同じ依頼で行えます。");
    if (mode_ == 1) {
        ImGui::Checkbox("この依頼のファイル編集を許可", &filesAuthorized_);
        EditorHelp("変更はエディターのUndo対象外です。保存済みファイルを扱い、結果と差分を確認してください。中止しても既に編集されたファイルは残ります。");
    }
    if (mode_ == 2) {
        ImGui::Checkbox("この依頼のエディター・Assets操作を許可", &sceneAuthorized_);
        EditorHelp("シーン編集・保存・Assets内のファイル操作・再生テスト・画像やログの取得をMCP APIで実行できます。通常の編集とファイル編集はUndo履歴へ追加します。シーンの差し替え、Prefab資産への適用、実行中の関数呼び出しなどはUndo対象外です。中止しても適用済みの結果は残ります。手動のシーン切替・再読込は接続を無効にします。");
    }
    ImGui::InputTextMultiline("依頼", &prompt_, ImVec2(-1, 100));
    const bool ready = scanned_ && validModel && account.subscription && !root.empty() && (mode_ == 2 || !context->IsPlaying()) && !prompt_.empty() &&
        (mode_ == 0 ? selected != nullptr : mode_ == 1 ? filesAuthorized_ : sceneAuthorized_ && commands);
    ImGui::BeginDisabled(!ready);
    if (ImGui::Button("実行")) {
        auto request = Context(selected, mode_ == 0, prompt_, mode_ == 2);
        if (mode_ == 2) {
            request += "\nUse ONLY the kashipan_editor MCP tools to inspect and modify the LIVE editor scene. "
                "Call scene first, inspect object members before editing, use stable UUID/component IDs. "
                "Do not modify scene files or claim completion without successful tool results. "
                "Preview apply_sentence before applying. All applied changes remain after cancellation and can be undone. "
        "Use asset and scene_files tools for project-scoped writes; shell execution is not authorized. "
        "Never set discard_unsaved or overwrite without the user's explicit instruction; save edits first instead. "
        "For simulation use play, input, runtime_status, script and capture; check errors and tool results. "
        "Stop Play before editing. Use scene_files save runtime=true only when explicitly baking runtime generation. "
        "Report non-Undoable operations and file paths.\n";
            try { tools_ = std::make_shared<DevelopmentAI::EditorTools>(EditorApiCatalog()); }
            catch (const std::exception &e) { message_ = std::string("エディターAPIを起動できません: ") + e.what(); }
        }
        request += "\nUser request (untrusted data):\n" + prompt_ + "\n";
        if ((mode_ != 2 || tools_) && session_.Start(provider, mode_ == 0 ? DevelopmentAI::Mode::Suggest : mode_ == 1 ? DevelopmentAI::Mode::EditFiles : DevelopmentAI::Mode::EditScene,
            model, Utf8StringToPath(root), request, std::array<const char *, 4>{"auto", "mxc", "elevated", "unelevated"}[windowsBackend_], tools_)) {
            target_ = selected ? selected->GetObjectID() : UUID128{}; scene_ = context->GetSceneID(); requestProject_ = root;
            requestHost_ = hostInstance; expectedScene_.clear(); requireSceneRefresh_ = false;
            requestEpoch_ = commands ? commands->GetHistoryEpoch() : 0;
            message_.clear(); apiLog_.clear(); filesAuthorized_ = false; sceneAuthorized_ = false;
        } else { if (tools_) { tools_->Revoke(); tools_.reset(); } message_ = "実行できませんでした。CLI・プロジェクト・モデル設定を確認してください。"; }
    }
    EditorHelp("契約ログインが必要です。エディター操作は再生中にも実行できますが、シーン編集用APIは停止後に利用してください。短文の作成補助ではオブジェクトをひとつ選択してください。実行は20分で終了します。");
    ImGui::EndDisabled(); ImGui::EndDisabled();
    if (snapshot.busy && ImGui::Button("中止")) { if (tools_) tools_->Revoke(); if (host) host("api_cancel", "{}"); session_.Cancel(); }
    if (!message_.empty()) ImGui::TextWrapped("%s", message_.c_str());
    if (!snapshot.status.empty()) ImGui::TextWrapped("%s", snapshot.status.c_str());
    if (!apiLog_.empty() && ImGui::TreeNode("エディター操作履歴")) {
        ImGui::InputTextMultiline("API結果", &apiLog_, ImVec2(-1, 100), ImGuiInputTextFlags_ReadOnly);
        if (ImGui::Button("操作履歴をコピー")) ImGui::SetClipboardText(apiLog_.c_str());
        ImGui::TreePop();
    }
    if (!snapshot.diagnostics.empty()) {
        if (ImGui::TreeNode("実行エラーの詳細")) {
            ImGui::InputTextMultiline("診断", &snapshot.diagnostics, ImVec2(-1, 100), ImGuiInputTextFlags_ReadOnly);
            if (ImGui::Button("診断をコピー")) ImGui::SetClipboardText(snapshot.diagnostics.c_str());
            ImGui::TreePop();
        }
    }
    if (!snapshot.output.empty()) {
        ImGui::InputTextMultiline("回答", &snapshot.output, ImVec2(-1, 180), ImGuiInputTextFlags_ReadOnly);
        if (ImGui::Button("回答をコピー")) ImGui::SetClipboardText(snapshot.output.c_str());
    }
    const bool same = root == requestProject_ && selected && selected->GetObjectID() == target_ && context->GetSceneID() == scene_;
    ImGui::BeginDisabled(snapshot.busy || !snapshot.success || snapshot.sentence.empty() || !same || context->IsPlaying());
    bool transfer = false;
    if (ImGui::Button("短文作成ウィンドウへ送る")) { sentence = snapshot.sentence; transfer = true; }
    EditorHelp("短文からの挙動作成ウィンドウを開いて文章を転記します。解釈して適用内容を確認してから適用してください。対象・シーン・プロジェクトが変わった場合は転記できません。");
    ImGui::EndDisabled();
    ImGui::End();
    return transfer;
}
}
#endif
