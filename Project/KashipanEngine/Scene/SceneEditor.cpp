#include "SceneEditor.h"
#ifdef USE_IMGUI
#include <algorithm>
#include <cctype>
#include <filesystem>
#include <imgui.h>
#include <string>
#include <Windows.h>

#include "Assets/AudioManager.h"
#include "Assets/MaterialManager.h"
#include "Core/GameEngine.h"
#include "Assets/ModelManager.h"
#include "Assets/TextureManager.h"
#include "Debug/Logger.h"
#include "Core/ProjectPaths.h"
#include "Graphics/PipelineManager.h"
#include "Input/Input.h"
#include "Input/InputCommand.h"
#include "Objects/Components/ScriptComponent.h"
#include "Objects/EmptyObject.h"
#include "Scene/Editor/AssetsWindow.h"
#include "Scene/Editor/SceneCrashRecovery.h"
#include "Scene/Editor/EditorKeyBindings.h"
#include "Scene/Editor/EditorPreferences.h"
#include "Scene/Editor/EditorSettings.h"
#include "Scene/Editor/PrefabAssetManager.h"
#include "Scene/Editor/PrefabUtility.h"
#include "Scene/Editor/ProjectWindow.h"
#include "Scene/Editor/PrefabSyncUtility.h"
#include "Scene/Editor/SceneComponentInspector.h"
#include "Scene/Editor/SceneEditorCommands.h"
#include "Scene/Editor/SceneEditorView.h"
#include "Scene/Editor/SceneListEditor.h"
#include "Scene/Editor/SceneLoder.h"
#include "Scene/Editor/SceneObjectHierarchy.h"
#include "Scene/Editor/SceneObjectInspector.h"
#include "Scene/Editor/SceneSaver.h"
#include "Scene/Editor/GlobalSceneVariablesMenu.h"
#include "Scene/Editor/SceneVariablesMenu.h"
#include "Utilities/ImGuiCustom.h"
#include "Utilities/Conversion/ConvertString.h"
#include "Scene/Editor/TranslationEditor.h"
#include "Scene/Components/Render/SceneRenderer.h"
#include "Scene/Components/Script/EditorToolManager.h"
#include "Scene/SceneBackupPath.h"
#include "Scene/SceneFileIO.h"
#include "Scene/SceneManager.h"
#include "Utilities/FileIO.h"
#include "Utilities/TemplateLiteral.h"
#include "Utilities/TimeUtils.h"
#include <iomanip>
#include <sstream>

namespace KashipanEngine {

namespace {
/// @brief 自動保存のファイル名をTemplateLiteralから構築する（拡張子が無ければ .json を付与する）
std::string RenderAutoSaveFileName(const std::string &nameFormat, const std::string &sceneName) {
    auto pad2 = [](int v) { std::ostringstream os; os << std::setw(2) << std::setfill('0') << v; return os.str(); };
    auto pad4 = [](int v) { std::ostringstream os; os << std::setw(4) << std::setfill('0') << v; return os.str(); };

    TemplateLiteral tpl(nameFormat);
    tpl.Set("SceneName", sceneName);
    const TimeRecord t = GetNowTime();
    tpl.Set("Year", pad4(t.year));
    tpl.Set("Month", pad2(t.month));
    tpl.Set("Day", pad2(t.day));
    tpl.Set("Hour", pad2(t.hour));
    tpl.Set("Minute", pad2(t.minute));
    tpl.Set("Second", pad2(static_cast<int>(t.second)));

    std::string name = tpl.Render();
    if (name.size() < 5 || name.substr(name.size() - 5) != ".json") {
        name += ".json";
    }
    return name;
}
} // namespace

SceneEditor::SceneEditor(Passkey<Scene>, SceneEditorContext *context) {
    context_ = context;
    commands_ = std::make_unique<SceneEditorCommands>(Passkey<SceneEditor>{}, context_);
    objectHierarchy_ = std::make_unique<SceneObjectHierarchy>(Passkey<SceneEditor>{}, context_);
    objectInspector_ = std::make_unique<SceneObjectInspector>(Passkey<SceneEditor>{}, context_, objectHierarchy_.get());
    componentInspector_ = std::make_unique<SceneComponentInspector>(Passkey<SceneEditor>{}, context_);
    variablesMenu_ = std::make_unique<SceneVariablesMenu>(Passkey<SceneEditor>{}, context_);
    globalVariablesMenu_ = std::make_unique<GlobalSceneVariablesMenu>(Passkey<SceneEditor>{}, context_);
    sceneView_ = std::make_unique<SceneEditorView>(Passkey<SceneEditor>{}, context_);
    assetsWindow_ = std::make_unique<AssetsWindow>(Passkey<SceneEditor>{}, context_);
    saver_ = std::make_unique<SceneSaver>(Passkey<SceneEditor>{}, context_);
    loader_ = std::make_unique<SceneLoader>(Passkey<SceneEditor>{}, context_);
    crashRecovery_ = std::make_unique<SceneCrashRecovery>(Passkey<SceneEditor>{}, context_);
    sceneListEditor_ = std::make_unique<SceneListEditor>(Passkey<SceneEditor>{}, context_);
    preferences_ = std::make_unique<EditorPreferences>(Passkey<SceneEditor>{});
    projectWindow_ = std::make_unique<ProjectWindow>(Passkey<SceneEditor>{});
    translationEditor_ = std::make_unique<TranslationEditor>(Passkey<SceneEditor>{});

    objectHierarchy_->SetCommands(commands_.get());
    objectInspector_->SetCommands(commands_.get());
    assetsWindow_->SetCommands(commands_.get());

    // 元Prefabがエンジン内操作で書き換わった際、シーン上の他インスタンスへ非破壊マージを伝播する
    // （シーン切り替えのたびにSceneEditorが再構築されるため、リスナーは常に最新のものだけが有効になるよう
    // PrefabAssetManager側で単一スロット置き換え方式にしている）
    PrefabAssetManager::SetChangeListener(
        [this](const UUID128 &prefabID, const JSON &oldJson, const JSON &newJson) {
            PrefabSyncUtility::SyncAllScenes(context_, commands_.get(), prefabID, oldJson, newJson);
        });

    // ウィンドウの表示状態を復元する（再起動後も維持される）
    isShowSceneView_ = EditorSettings::GetBool("sceneEditor.showSceneView", true);
    isShowHierarchy_ = EditorSettings::GetBool("sceneEditor.showHierarchy", true);
    isShowObjectInspector_ = EditorSettings::GetBool("sceneEditor.showObjectInspector", true);
    isShowComponentInspector_ = EditorSettings::GetBool("sceneEditor.showComponentInspector", true);
    isShowVariablesMenu_ = EditorSettings::GetBool("sceneEditor.showVariablesMenu", true);
    isShowGlobalVariablesMenu_ = EditorSettings::GetBool("sceneEditor.showGlobalVariablesMenu", true);
    isShowAssets_ = EditorSettings::GetBool("sceneEditor.showAssets", true);
    isShowSceneList_ = EditorSettings::GetBool("sceneEditor.showSceneList", true);
    isShowHistory_ = EditorSettings::GetBool("sceneEditor.showHistory", true);
    isShowPreferences_ = EditorSettings::GetBool("sceneEditor.showPreferences", false);
    isShowProjectWindow_ = EditorSettings::GetBool("sceneEditor.showProjectWindow", false);
    isShowTranslationEditor_ = EditorSettings::GetBool("sceneEditor.showTranslationEditor", false);

    isShowLoadedTexturesWindow_ = EditorSettings::GetBool("sceneEditor.showLoadedTextures", false);
    isShowLoadedModelsWindow_ = EditorSettings::GetBool("sceneEditor.showLoadedModels", false);
    isShowMaterialsWindow_ = EditorSettings::GetBool("sceneEditor.showMaterials", false);
    isShowLoadedSoundsWindow_ = EditorSettings::GetBool("sceneEditor.showLoadedSounds", false);
    isShowPlayingSoundsWindow_ = EditorSettings::GetBool("sceneEditor.showPlayingSounds", false);
    isShowLoggerWindow_ = EditorSettings::GetBool("sceneEditor.showLogger", true);
    isShowInputStateWindow_ = EditorSettings::GetBool("sceneEditor.showInputState", false);
    isShowInputCommandEditorWindow_ = EditorSettings::GetBool("sceneEditor.showInputCommandEditor", false);

    // 自動保存の設定を復元する（再起動後も維持される）
    autoSaveIntervalMinutes_ = EditorSettings::GetFloat("sceneEditor.autoSaveIntervalMinutes", 1.0f);
    autoSaveNameFormat_ = EditorSettings::GetString("sceneEditor.autoSaveNameFormat", "${SceneName}");

    InitializeExternalAssetSnapshot();
    wasEditorApplicationActive_ = IsEditorApplicationActive();
}

SceneEditor::~SceneEditor() = default;

void SceneEditor::InitializeExternalAssetSnapshot() {
    externalAssetSnapshot_.clear();
    const std::string &assetsRoot = ProjectPaths::AssetsRoot();
    if (assetsRoot.empty()) return;

    std::error_code ec;
    const std::filesystem::path root = Utf8StringToPath(assetsRoot);
    if (!std::filesystem::is_directory(root, ec)) return;
    for (std::filesystem::recursive_directory_iterator it(root, std::filesystem::directory_options::skip_permission_denied, ec), end;
         it != end; it.increment(ec)) {
        if (ec) { ec.clear(); continue; }
        if (!it->is_regular_file(ec) || ec) { ec.clear(); continue; }
        const std::string logicalPath = ProjectPaths::ToLogical(PathToUtf8String(it->path()));
        externalAssetSnapshot_[logicalPath] = { it->last_write_time(ec), it->file_size(ec) };
        ec.clear();
    }

    // 外部更新前のPrefab JSONを保持し、更新後との差分伝播に使える状態にしておく。
    for (const std::string &path : ProjectPaths::ListAssetFiles({ PrefabUtility::kPrefabExtension })) {
        PrefabAssetManager::GetPrefabIDFromPath(path);
    }
}

bool SceneEditor::IsEditorApplicationActive() const {
    const HWND foregroundWindow = GetForegroundWindow();
    if (!foregroundWindow) return false;
    DWORD foregroundProcessID = 0;
    GetWindowThreadProcessId(foregroundWindow, &foregroundProcessID);
    return foregroundProcessID == GetCurrentProcessId();
}

void SceneEditor::PollExternalAssetChanges() {
    const bool isActive = IsEditorApplicationActive();
    const bool becameActive = isActive && !wasEditorApplicationActive_;
    wasEditorApplicationActive_ = isActive;
    if (!isActive) return;

    const float deltaTime = GetDeltaTime();
    externalAssetPollElapsed_ += deltaTime;
    if (!pendingExternalAssetPaths_.empty()) externalAssetDebounceElapsed_ += deltaTime;

    // Unityと同様に、外部ツールからエディターへ戻った時点で即座に変更を確認する。
    // アクティブ中の低頻度監視も残し、ウィンドウを並べて編集する場合の変更も拾う。
    if (becameActive || externalAssetPollElapsed_ >= 1.0f) {
        externalAssetPollElapsed_ = 0.0f;
        std::unordered_map<std::string, ExternalFileStamp> current;
        std::error_code ec;
        const std::string &assetsRoot = ProjectPaths::AssetsRoot();
        if (assetsRoot.empty()) return;
        const std::filesystem::path root = Utf8StringToPath(assetsRoot);
        if (std::filesystem::is_directory(root, ec)) {
            for (std::filesystem::recursive_directory_iterator it(root, std::filesystem::directory_options::skip_permission_denied, ec), end;
                 it != end; it.increment(ec)) {
                if (ec) { ec.clear(); continue; }
                if (!it->is_regular_file(ec) || ec) { ec.clear(); continue; }
                const std::string logicalPath = ProjectPaths::ToLogical(PathToUtf8String(it->path()));
                ExternalFileStamp stamp{ it->last_write_time(ec), it->file_size(ec) };
                ec.clear();
                current[logicalPath] = stamp;
                auto old = externalAssetSnapshot_.find(logicalPath);
                if (old == externalAssetSnapshot_.end() || !(old->second == stamp)) {
                    pendingExternalAssetPaths_.insert(logicalPath);
                    externalAssetDebounceElapsed_ = 0.0f;
                }
            }
            for (const auto &[path, stamp] : externalAssetSnapshot_) {
                if (!current.contains(path)) {
                    pendingExternalAssetPaths_.insert(path);
                    externalAssetDebounceElapsed_ = 0.0f;
                }
            }
            externalAssetSnapshot_ = std::move(current);
        }
    }

    if (!externalSceneChangeRequested_ && !pendingExternalAssetPaths_.empty() && externalAssetDebounceElapsed_ >= 0.75f) {
        std::vector<std::string> changed(pendingExternalAssetPaths_.begin(), pendingExternalAssetPaths_.end());
        pendingExternalAssetPaths_.clear();
        externalAssetDebounceElapsed_ = 0.0f;
        std::sort(changed.begin(), changed.end());
        ProcessExternalAssetChanges(std::move(changed));
    }
}

std::string SceneEditor::GetCurrentSceneFilePath() const {
    const SceneManager *sceneManager = context_->GetSceneManager();
    if (!sceneManager) return {};
    for (const auto &entry : sceneManager->GetRegisteredScenes()) {
        if (entry.name == context_->GetName()) return ProjectPaths::NormalizeSeparators(entry.filePath);
    }
    return {};
}

void SceneEditor::ProcessExternalAssetChanges(std::vector<std::string> changedPaths) {
    assetsWindow_->RefreshAfterExternalChanges();
    const std::string scenePath = GetCurrentSceneFilePath();
    const std::string scenePrefix = scenePath.empty() || scenePath.ends_with('/') ? scenePath : scenePath + "/";
    const bool currentSceneChanged = !scenePath.empty() && std::ranges::any_of(changedPaths, [&](const std::string &path) {
        return path == scenePath || (!scenePrefix.empty() && path.starts_with(scenePrefix));
    });

    if (currentSceneChanged) {
        JSON diskScene = LoadSceneFromPath(scenePath);
        if (!diskScene.is_discarded() && diskScene == context_->SaveSceneToJSON()) {
            ProcessNonSceneExternalChanges(changedPaths);
            return;
        }
        deferredExternalAssetPaths_ = std::move(changedPaths);
        externallyChangedScenePath_ = scenePath;
        externallyChangedSceneJson_ = std::move(diskScene);
        externalSceneCanReload_ = !externallyChangedSceneJson_.is_discarded() && externallyChangedSceneJson_.is_object();
        externalSceneChangeRequested_ = true;
        return;
    }
    ProcessNonSceneExternalChanges(changedPaths);
}

void SceneEditor::ProcessNonSceneExternalChanges(const std::vector<std::string> &changedPaths) {
    if (context_->IsPlaying()) {
        // 実行中のオブジェクト、スクリプトインスタンス、描画パイプラインを途中で
        // 差し替えない。PlayStopによる編集状態の復元後にまとめて適用する。
        deferredPlayModeAssetPaths_.insert(changedPaths.begin(), changedPaths.end());
        return;
    }

    PrefabAssetManager::ReloadExternallyChangedFiles(changedPaths);

    std::unordered_set<std::string> changedScripts;
    bool pipelineChanged = false;
    for (const std::string &path : changedPaths) {
        std::string extension = PathToUtf8String(Utf8StringToPath(path).extension());
        std::ranges::transform(extension, extension.begin(), [](unsigned char c) { return static_cast<char>(std::tolower(c)); });
        if (extension == ".as") changedScripts.insert(path);
        if (extension == ".hlsl" || extension == ".hlsli") pipelineChanged = true;
    }

    if (!changedScripts.empty()) {
        for (EmptyObject *object : context_->GetSceneObjects()) {
            if (!object) continue;
            for (const auto &[component, index] : object->GetAllComponents()) {
                auto *script = dynamic_cast<ScriptComponent *>(component);
                if (script && changedScripts.contains(ProjectPaths::NormalizeSeparators(script->GetScriptPath()))) {
                    script->ReloadFromDisk();
                }
            }
        }
    }
    if (pipelineChanged) PipelineManager::TryReloadPipelines();
}

void SceneEditor::ShowExternalSceneChangeModal() {
    const std::string popupTitle = Translation("editor.externalchanges.title") + "##SceneEditorExternalChange";
    if (externalSceneChangeRequested_) ImGui::OpenPopup(popupTitle.c_str());
    if (!ImGui::BeginPopupModal(popupTitle.c_str(), nullptr, ImGuiWindowFlags_AlwaysAutoResize)) return;

    ImGui::TextWrapped("%s", TranslationC("editor.externalchanges.message"));
    ImGui::Separator();
    ImGui::TextWrapped("%s: %s", TranslationC("editor.externalchanges.scene"), externallyChangedScenePath_.c_str());
    if (context_->IsPlaying()) ImGui::TextWrapped("%s", TranslationC("editor.externalchanges.playing"));

    ImGui::BeginDisabled(!externalSceneCanReload_ || context_->IsPlaying());
    if (ImGui::Button(TranslationLabel("editor.externalchanges.reload"))) {
        const JSON localBackup = context_->SaveSceneToJSON();
        if (context_->LoadSceneFromJSON(externallyChangedSceneJson_)) {
            commands_->Clear();
            objectHierarchy_->ClearSelection();
            ProcessNonSceneExternalChanges(deferredExternalAssetPaths_);
            deferredExternalAssetPaths_.clear();
            externalSceneChangeRequested_ = false;
            ImGui::CloseCurrentPopup();
        } else {
            context_->LoadSceneFromJSON(localBackup);
        }
    }
    ImGui::EndDisabled();
    ImGui::SameLine();
    if (ImGui::Button(TranslationLabel("editor.externalchanges.keeplocal"))) {
        ProcessNonSceneExternalChanges(deferredExternalAssetPaths_);
        deferredExternalAssetPaths_.clear();
        externalSceneChangeRequested_ = false;
        ImGui::CloseCurrentPopup();
    }
    ImGui::EndPopup();
}

void SceneEditor::ShowImGui() {
    // スクリプトやシーンからのゲームループ終了要求は、エディター上では再生停止として消費する
    // （エディター自体は閉じない。Stopボタンと同様にUUIDで選択を退避してから復元する）
    if (GameEngine::IsExitGameLoopRequested()) {
        GameEngine::ClearExitGameLoopRequest();
        if (context_->IsPlaying()) {
            const auto selectedIDs = objectHierarchy_->GetSelectedObjectIDs();
            context_->PlayStop();
            objectHierarchy_->RestoreSelection(selectedIDs);
        }
    }

    // 再生状態の変化を監視して、コマンド履歴の再生セッションを切り替える。
    // 再生中に積まれたコマンドは停止時のシーン復元と整合しないため、停止時にまとめて破棄され、
    // 編集時のUndo/Redo履歴は再生を跨いでもそのまま使用できる。
    // （ボタン以外の経路で再生状態が変わっても追従できるよう、フレーム毎の状態変化で検知する）
    const bool isPlaying = context_->IsPlaying();
    if (isPlaying != wasPlaying_) {
        if (isPlaying) {
            commands_->BeginPlaySession();
        } else {
            commands_->EndPlaySession();
            if (!deferredPlayModeAssetPaths_.empty()) {
                std::vector<std::string> deferred(
                    deferredPlayModeAssetPaths_.begin(), deferredPlayModeAssetPaths_.end());
                deferredPlayModeAssetPaths_.clear();
                std::sort(deferred.begin(), deferred.end());
                ProcessNonSceneExternalChanges(deferred);
            }
        }
        wasPlaying_ = isPlaying;
    }

    // エディターツールスクリプトの読み込み（初回のみ）と、ツール内GetScene()用のシーンコンテキスト設定
    EditorToolManager::GetInstance().BeginFrame(context_->GetSceneContext());

    PollExternalAssetChanges();
    ShowMainWindow();
    ShowExternalSceneChangeModal();
    HandleShortcuts();
    HandleAutoSave();

    // エディターツールスクリプトのウィンドウ表示・Update呼び出し
    EditorToolManager::GetInstance().UpdateTools();

    // スクリプトや再生状態の復元によってUI描画前にオブジェクトが削除される場合がある。
    // インスペクターやシーンビューへ選択ポインターを渡す前に、現在のシーンに存在するものだけへ絞る。
    objectHierarchy_->ValidateCachedObjects();

    if (isShowHierarchy_) objectHierarchy_->ShowImGui();
    if (isShowObjectInspector_) objectInspector_->ShowImGui();
    if (isShowComponentInspector_) componentInspector_->ShowImGui();
    if (isShowVariablesMenu_) variablesMenu_->ShowImGui();
    if (isShowGlobalVariablesMenu_) globalVariablesMenu_->ShowImGui();
    if (isShowSceneView_) sceneView_->ShowImGui(objectHierarchy_->GetSelectedObjects(), commands_.get(), objectHierarchy_.get());
    if (isShowAssets_) assetsWindow_->ShowImGui();
    if (isShowSceneList_) sceneListEditor_->ShowImGui();
    if (isShowPreferences_) preferences_->ShowImGui();
    if (isShowProjectWindow_) projectWindow_->ShowImGui();
    if (isShowTranslationEditor_) translationEditor_->ShowImGui();

    //--------- デバッグ用ウィンドウ（旧ImGuiManagerから移設） ---------//
    if (isShowLoadedTexturesWindow_) TextureManager::ShowImGuiLoadedTexturesWindow();
    if (isShowLoadedModelsWindow_) ModelManager::ShowImGuiLoadedModelsWindow(context_->GetSceneContext());
    if (isShowMaterialsWindow_) MaterialManager::ShowImGuiMaterialManagerWindow();
    if (isShowLoadedSoundsWindow_) AudioManager::ShowImGuiLoadedSoundsWindow();
    if (isShowPlayingSoundsWindow_) AudioManager::ShowImGuiPlayingSoundsWindow();
    if (isShowLoggerWindow_) ShowImGuiLoggerWindow(Passkey<SceneEditor>{});
    if (isShowImGuiDemoWindow_) ImGui::ShowDemoWindow(&isShowImGuiDemoWindow_);
    if (isShowInputStateWindow_) {
        if (auto *input = context_->GetInput()) input->ShowImGui();
    }
    if (isShowInputCommandEditorWindow_) {
        if (auto *inputCommand = context_->GetInputCommand()) inputCommand->ShowImGui();
    }
}

void SceneEditor::ShowMainWindow() {
    //--------- メインメニューバー（保存・読込・Undo/Redo・ウィンドウ切替・デバッグウィンドウ） ---------//
    if (ImGui::BeginMainMenuBar()) {
        if (ImGui::BeginMenu(TranslationLabel("editor.menu.file"))) {
            if (ImGui::MenuItem(TranslationLabel("editor.menu.file.project"), nullptr, &isShowProjectWindow_)) {
                EditorSettings::SetBool("sceneEditor.showProjectWindow", isShowProjectWindow_);
            }
            ImGui::SetItemTooltip("%s", TranslationC("editor.menu.file.project.tooltip"));
            ImGui::Separator();
            if (ImGui::MenuItem(TranslationLabel("editor.menu.file.newscene"))) {
                isNewSceneRequested_ = true;
                newSceneName_ = "New Scene";
            }
            const std::string saveSceneShortcut = EditorKeyBindings::ToDisplayString(EditorKeyBindings::Get("SaveScene", ImGuiMod_Ctrl | ImGuiKey_S));
            if (ImGui::MenuItem(TranslationLabel("editor.menu.file.savescene"), saveSceneShortcut.c_str())) {
                saver_->Open();
            }
            if (ImGui::MenuItem(TranslationLabel("editor.menu.file.loadscene"))) {
                loader_->Open();
            }
            ImGui::Separator();
            if (ImGui::MenuItem(TranslationLabel("editor.menu.file.autosavesettings"))) {
                isAutoSaveSettingsRequested_ = true;
            }
            ImGui::EndMenu();
        }
        if (ImGui::BeginMenu(TranslationLabel("editor.menu.edit"))) {
            const std::string undoLabel = Translation("editor.menu.edit.undo") + (commands_->CanUndo() ? (" " + commands_->GetUndoName()) : "");
            const std::string redoLabel = Translation("editor.menu.edit.redo") + (commands_->CanRedo() ? (" " + commands_->GetRedoName()) : "");
            const std::string undoShortcut = EditorKeyBindings::ToDisplayString(EditorKeyBindings::Get("Undo", ImGuiMod_Ctrl | ImGuiKey_Z));
            const std::string redoShortcut = EditorKeyBindings::ToDisplayString(EditorKeyBindings::Get("Redo", ImGuiMod_Ctrl | ImGuiKey_Y));
            if (ImGui::MenuItem(undoLabel.c_str(), undoShortcut.c_str(), false, commands_->CanUndo())) {
                PerformUndo();
            }
            if (ImGui::MenuItem(redoLabel.c_str(), redoShortcut.c_str(), false, commands_->CanRedo())) {
                PerformRedo();
            }
            ImGui::Separator();
            if (ImGui::MenuItem(TranslationLabel("editor.menu.edit.preferences"), nullptr, &isShowPreferences_)) {
                EditorSettings::SetBool("sceneEditor.showPreferences", isShowPreferences_);
            }
            if (ImGui::MenuItem(TranslationLabel("editor.translationeditor.window"), nullptr, &isShowTranslationEditor_)) {
                EditorSettings::SetBool("sceneEditor.showTranslationEditor", isShowTranslationEditor_);
            }
            ImGui::SetItemTooltip("%s", TranslationC("editor.menu.edit.translationeditor.tooltip"));
            if (ImGui::MenuItem(TranslationLabel("editor.inputcommand.window"), nullptr, &isShowInputCommandEditorWindow_)) {
                EditorSettings::SetBool("sceneEditor.showInputCommandEditor", isShowInputCommandEditorWindow_);
            }
            ImGui::EndMenu();
        }
        if (ImGui::BeginMenu(TranslationLabel("editor.menu.window"))) {
            if (ImGui::MenuItem(TranslationLabel("editor.sceneview.window"), nullptr, &isShowSceneView_)) {
                EditorSettings::SetBool("sceneEditor.showSceneView", isShowSceneView_);
            }
            if (ImGui::MenuItem(TranslationLabel("editor.sceneobjecthierarchy.window"), nullptr, &isShowHierarchy_)) {
                EditorSettings::SetBool("sceneEditor.showHierarchy", isShowHierarchy_);
            }
            if (ImGui::MenuItem(TranslationLabel("editor.sceneobjectinspector.window"), nullptr, &isShowObjectInspector_)) {
                EditorSettings::SetBool("sceneEditor.showObjectInspector", isShowObjectInspector_);
            }
            if (ImGui::MenuItem(TranslationLabel("editor.scenecomponentinspector.window"), nullptr, &isShowComponentInspector_)) {
                EditorSettings::SetBool("sceneEditor.showComponentInspector", isShowComponentInspector_);
            }
            if (ImGui::MenuItem(TranslationLabel("editor.scenevariables.window"), nullptr, &isShowVariablesMenu_)) {
                EditorSettings::SetBool("sceneEditor.showVariablesMenu", isShowVariablesMenu_);
            }
            if (ImGui::MenuItem(TranslationLabel("editor.globalscenevariables.window"), nullptr, &isShowGlobalVariablesMenu_)) {
                EditorSettings::SetBool("sceneEditor.showGlobalVariablesMenu", isShowGlobalVariablesMenu_);
            }
            if (ImGui::MenuItem(TranslationLabel("editor.assets.window"), nullptr, &isShowAssets_)) {
                EditorSettings::SetBool("sceneEditor.showAssets", isShowAssets_);
            }
            if (ImGui::MenuItem(TranslationLabel("editor.scenelist.window"), nullptr, &isShowSceneList_)) {
                EditorSettings::SetBool("sceneEditor.showSceneList", isShowSceneList_);
            }
            if (ImGui::MenuItem(TranslationLabel("editor.history.window"), nullptr, &isShowHistory_)) {
                EditorSettings::SetBool("sceneEditor.showHistory", isShowHistory_);
            }
            ImGui::EndMenu();
        }
        if (ImGui::BeginMenu(TranslationLabel("editor.menu.debugwindows"))) {
            if (ImGui::MenuItem(TranslationLabel("editor.texturemanager.window"), nullptr, &isShowLoadedTexturesWindow_)) {
                EditorSettings::SetBool("sceneEditor.showLoadedTextures", isShowLoadedTexturesWindow_);
            }
            if (ImGui::MenuItem(TranslationLabel("editor.modelmanager.window"), nullptr, &isShowLoadedModelsWindow_)) {
                EditorSettings::SetBool("sceneEditor.showLoadedModels", isShowLoadedModelsWindow_);
            }
            if (ImGui::MenuItem(TranslationLabel("editor.materialmanager.window"), nullptr, &isShowMaterialsWindow_)) {
                EditorSettings::SetBool("sceneEditor.showMaterials", isShowMaterialsWindow_);
            }
            if (ImGui::MenuItem(TranslationLabel("editor.audiomanager.loaded.window"), nullptr, &isShowLoadedSoundsWindow_)) {
                EditorSettings::SetBool("sceneEditor.showLoadedSounds", isShowLoadedSoundsWindow_);
            }
            if (ImGui::MenuItem(TranslationLabel("editor.audiomanager.playing.window"), nullptr, &isShowPlayingSoundsWindow_)) {
                EditorSettings::SetBool("sceneEditor.showPlayingSounds", isShowPlayingSoundsWindow_);
            }
            if (ImGui::MenuItem(TranslationLabel("editor.logger.window"), nullptr, &isShowLoggerWindow_)) {
                EditorSettings::SetBool("sceneEditor.showLogger", isShowLoggerWindow_);
            }
            ImGui::Separator();
            if (ImGui::MenuItem(TranslationLabel("editor.menu.debugwindows.reloadpipelines"))) {
                PipelineManager::TryReloadPipelines();
            }
            ImGui::Separator();
            if (ImGui::MenuItem(TranslationLabel("editor.input.state.window"), nullptr, &isShowInputStateWindow_)) {
                EditorSettings::SetBool("sceneEditor.showInputState", isShowInputStateWindow_);
            }
            ImGui::Separator();
            ImGui::MenuItem(TranslationLabel("editor.menu.debugwindows.imguidemo"), nullptr, &isShowImGuiDemoWindow_);
            ImGui::EndMenu();
        }
        // エディターツールスクリプトの[MenuItem("MenuBar/...")]で追加された項目
        EditorToolManager::GetInstance().ShowMenuBarItems();
        ImGui::EndMainMenuBar();
    }

    if (!ImGui::Begin(TranslationLabel("editor.sceneeditor.window"))) {
        ImGui::End();
        return;
    }

    //--------- シーン名の編集 ---------//
    std::string sceneName = context_->GetName();
    if (ImGui::InputText(TranslationLabel("editor.sceneeditor.scenename"), &sceneName, ImGuiInputTextFlags_EnterReturnsTrue)) {
        context_->SetName(sceneName);
    }

    //--------- 再生制御（Unity風のPlay/Pause/Stop） ---------//
    ShowPlayControls();

    //--------- 新規作成・保存・読込ポップアップ ---------//
    if (ShowNewSceneModal()) {
        // シーンが差し替わったので選択と履歴をクリアする
        commands_->Clear();
        objectHierarchy_->ClearSelection();
    }
    saver_->ShowImGui();
    if (loader_->ShowImGui()) {
        // シーンが差し替わったので選択と履歴をクリアする
        commands_->Clear();
        objectHierarchy_->ClearSelection();
    }
    if (crashRecovery_->ShowImGui()) {
        // シーンが差し替わったので選択と履歴をクリアする
        commands_->Clear();
        objectHierarchy_->ClearSelection();
    }
    ShowAutoSaveSettingsModal();

    ImGui::End();

    //--------- 操作履歴 ---------//
    if (isShowHistory_) {
        const bool wasOpen = isShowHistory_;
        if (ImGui::Begin(TranslationLabel("editor.history.window"), &isShowHistory_)) {
            commands_->ShowHistoryImGui();
        }
        ImGui::End();
        if (wasOpen != isShowHistory_) {
            EditorSettings::SetBool("sceneEditor.showHistory", isShowHistory_);
        }
    }
}

void SceneEditor::ShowPlayControls() {
    ImGui::Separator();
    if (!context_->IsPlaying()) {
        if (ImGui::Button(TranslationLabel("editor.play.play"))) {
            // 再生開始でオブジェクトのポインタ等が変わりうるため、UUIDで選択を控えて復元する
            const auto selectedIDs = objectHierarchy_->GetSelectedObjectIDs();
            // PlayStart()はEditorOnlyオブジェクトを削除してしまうため、それより前の
            // （EditorOnlyオブジェクトを含む）編集状態をバックアップしておく。
            // 再生中のクラッシュ等で落ちた場合、このバックアップから再生直前の編集状態へ戻せる。
            // 一定間隔の自動バックアップ（Stopped_/Playing_）とは区別できる専用プレフィックスを使う
            TakeSceneBackup("PlayStart_");
            context_->PlayStart();
            objectHierarchy_->RestoreSelection(selectedIDs);
        }
    } else {
        if (ImGui::Button(TranslationLabel("editor.play.stop"))) {
            // 停止時はスナップショットからシーンが再構築されるため、UUIDで選択を復元する
            const auto selectedIDs = objectHierarchy_->GetSelectedObjectIDs();
            context_->PlayStop();
            objectHierarchy_->RestoreSelection(selectedIDs);
        }
        ImGui::SameLine();
        if (context_->IsPaused()) {
            if (ImGui::Button(TranslationLabel("editor.play.resume"))) {
                context_->PlayResume();
            }
        } else {
            if (ImGui::Button(TranslationLabel("editor.play.pause"))) {
                context_->PlayPause();
            }
        }
        ImGui::SameLine();
        ImGui::BeginDisabled(!context_->IsPaused());
        if (ImGui::Button(TranslationLabel("editor.play.stepframe"))) {
            context_->RequestStepFrame();
        }
        ImGui::EndDisabled();
    }
}

bool SceneEditor::ShowNewSceneModal() {
    bool created = false;
    if (isNewSceneRequested_) {
        ImGui::OpenPopup(TranslationLabel("editor.newscene.title"));
        isNewSceneRequested_ = false;
    }
    if (ImGui::BeginPopupModal(TranslationLabel("editor.newscene.title"), nullptr, ImGuiWindowFlags_AlwaysAutoResize)) {
        ImGui::TextColored(ImVec4(1.0f, 0.7f, 0.3f, 1.0f), "%s", TranslationC("editor.newscene.warning"));
        ImGui::InputText(TranslationLabel("editor.sceneeditor.scenename"), &newSceneName_);
        ImGui::Checkbox(TranslationLabel("editor.newscene.registertolist"), &newSceneRegisterToList_);
        ImGui::SetItemTooltip("%s", TranslationC("editor.newscene.registertolist.tooltip"));
        if (ImGui::Button(TranslationLabel("editor.common.create"), ImVec2(120, 0))) {
            const std::string sceneName = newSceneName_.empty() ? "New Scene" : newSceneName_;
            context_->ClearSceneObjects();
            context_->ClearSceneComponents();
            // 空のシーンでもシーンビューの背景とグリッドを毎フレーム描画できるよう、
            // 描画を担当する標準コンポーネントは作り直す。
            context_->AddComponent<SceneRenderer>();
            context_->SetName(sceneName);
            if (newSceneRegisterToList_) {
                auto *sceneManager = context_->GetSceneManager();
                if (sceneManager) {
                    const std::string filePath = "Assets/Scenes/" + sceneName + ".scene";
                    SaveSceneToPath(context_->SaveSceneToJSON(), filePath);
                    if (sceneManager->RegisterSceneFile(sceneName, filePath)) {
                        sceneManager->SaveSceneList();
                    }
                }
            }
            created = true;
            ImGui::CloseCurrentPopup();
        }
        ImGui::SameLine();
        if (ImGui::Button(TranslationLabel("editor.common.cancel"), ImVec2(120, 0))) {
            ImGui::CloseCurrentPopup();
        }
        ImGui::EndPopup();
    }
    return created;
}

void SceneEditor::HandleShortcuts() {
    if (ImGui::GetIO().WantTextInput) return;
    if (ImGui::IsKeyChordPressed(EditorKeyBindings::Get("Undo", ImGuiMod_Ctrl | ImGuiKey_Z))) {
        PerformUndo();
    }
    if (ImGui::IsKeyChordPressed(EditorKeyBindings::Get("Redo", ImGuiMod_Ctrl | ImGuiKey_Y))) {
        PerformRedo();
    }
    if (ImGui::IsKeyChordPressed(EditorKeyBindings::Get("SaveScene", ImGuiMod_Ctrl | ImGuiKey_S))) {
        saver_->Open();
    }
}

void SceneEditor::PerformUndo() {
    // オブジェクトの削除/再生成でポインタが変わる可能性があるため、
    // UUIDで選択を控えてから実行し、実行後にUUIDから再解決して選択を復元する
    const auto selectedIDs = objectHierarchy_->GetSelectedObjectIDs();
    commands_->Undo();
    objectHierarchy_->RestoreSelection(selectedIDs);
}

void SceneEditor::PerformRedo() {
    const auto selectedIDs = objectHierarchy_->GetSelectedObjectIDs();
    commands_->Redo();
    objectHierarchy_->RestoreSelection(selectedIDs);
}

void SceneEditor::HandleAutoSave() {
    float intervalMinutes = autoSaveIntervalMinutes_;
    if (intervalMinutes < 0.1f) intervalMinutes = 0.1f;

    autoSaveElapsedTime_ += GetDeltaTime();
    if (autoSaveElapsedTime_ < intervalMinutes * 60.0f) return;
    autoSaveElapsedTime_ = 0.0f;

    // 再生中かどうかでファイル名の先頭を変え、一覧で見分けやすくする
    TakeSceneBackup(context_->IsPlaying() ? "Playing_" : "Stopped_");
}

void SceneEditor::TakeSceneBackup(const std::string &prefix) {
    const std::string fileName = prefix + RenderAutoSaveFileName(autoSaveNameFormat_, context_->GetName());
    const std::string filePath = kSceneBackupDirectory + fileName;
    SaveJSON(context_->SaveSceneToJSON(), filePath);
}

void SceneEditor::ShowAutoSaveSettingsModal() {
    if (isAutoSaveSettingsRequested_) {
        ImGui::OpenPopup(TranslationLabel("editor.autosave.title"));
        isAutoSaveSettingsRequested_ = false;
    }
    if (ImGui::BeginPopupModal(TranslationLabel("editor.autosave.title"), nullptr, ImGuiWindowFlags_AlwaysAutoResize)) {
        if (ImGui::DragFloat(TranslationLabel("editor.autosave.interval"), &autoSaveIntervalMinutes_, 0.1f, 0.1f, 120.0f, "%.1f")) {
            EditorSettings::SetFloat("sceneEditor.autoSaveIntervalMinutes", autoSaveIntervalMinutes_);
        }
        if (ImGui::InputText(TranslationLabel("editor.autosave.nameformat"), &autoSaveNameFormat_)) {
            EditorSettings::SetString("sceneEditor.autoSaveNameFormat", autoSaveNameFormat_);
        }
        ImGuiCustom::TextDisabledWrapped("%s${SceneName} ${Year} ${Month} ${Day} ${Hour} ${Minute} ${Second}", TranslationC("editor.autosave.placeholders"));

        const std::string preview = RenderAutoSaveFileName(autoSaveNameFormat_, context_->GetName());
        ImGui::Text("%s%s%s", TranslationC("editor.autosave.preview"), kSceneBackupDirectory, preview.c_str());

        if (ImGui::Button(TranslationLabel("editor.common.close"), ImVec2(120, 0))) {
            ImGui::CloseCurrentPopup();
        }
        ImGui::EndPopup();
    }
}

} // namespace KashipanEngine
#endif // USE_IMGUI
