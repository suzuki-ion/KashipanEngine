#include "SceneEditor.h"
#ifdef USE_IMGUI
#include <imgui.h>
#include <string>

#include "Assets/AudioManager.h"
#include "Assets/MaterialManager.h"
#include "Assets/ModelManager.h"
#include "Assets/TextureManager.h"
#include "Debug/Logger.h"
#include "Input/Input.h"
#include "Input/InputCommand.h"
#include "Scene/Editor/AssetsWindow.h"
#include "Scene/Editor/EditorSettings.h"
#include "Scene/Editor/SceneComponentInspector.h"
#include "Scene/Editor/SceneEditorCommands.h"
#include "Scene/Editor/SceneEditorView.h"
#include "Scene/Editor/SceneLoder.h"
#include "Scene/Editor/SceneObjectHierarchy.h"
#include "Scene/Editor/SceneObjectInspector.h"
#include "Scene/Editor/SceneSaver.h"
#include "Scene/Editor/SceneVariablesMenu.h"

namespace KashipanEngine {

SceneEditor::SceneEditor(Passkey<Scene>, SceneEditorContext *context) {
    context_ = context;
    commands_ = std::make_unique<SceneEditorCommands>(Passkey<SceneEditor>{}, context_);
    objectHierarchy_ = std::make_unique<SceneObjectHierarchy>(Passkey<SceneEditor>{}, context_);
    objectInspector_ = std::make_unique<SceneObjectInspector>(Passkey<SceneEditor>{}, context_, objectHierarchy_.get());
    componentInspector_ = std::make_unique<SceneComponentInspector>(Passkey<SceneEditor>{}, context_);
    variablesMenu_ = std::make_unique<SceneVariablesMenu>(Passkey<SceneEditor>{}, context_);
    sceneView_ = std::make_unique<SceneEditorView>(Passkey<SceneEditor>{}, context_);
    assetsWindow_ = std::make_unique<AssetsWindow>(Passkey<SceneEditor>{});
    saver_ = std::make_unique<SceneSaver>(Passkey<SceneEditor>{}, context_);
    loader_ = std::make_unique<SceneLoader>(Passkey<SceneEditor>{}, context_);

    objectHierarchy_->SetCommands(commands_.get());
    objectInspector_->SetCommands(commands_.get());

    // ウィンドウの表示状態を復元する（再起動後も維持される）
    isShowSceneView_ = EditorSettings::GetBool("sceneEditor.showSceneView", true);
    isShowHierarchy_ = EditorSettings::GetBool("sceneEditor.showHierarchy", true);
    isShowObjectInspector_ = EditorSettings::GetBool("sceneEditor.showObjectInspector", true);
    isShowComponentInspector_ = EditorSettings::GetBool("sceneEditor.showComponentInspector", true);
    isShowVariablesMenu_ = EditorSettings::GetBool("sceneEditor.showVariablesMenu", true);
    isShowAssets_ = EditorSettings::GetBool("sceneEditor.showAssets", true);

    isShowLoadedTexturesWindow_ = EditorSettings::GetBool("sceneEditor.showLoadedTextures", false);
    isShowLoadedModelsWindow_ = EditorSettings::GetBool("sceneEditor.showLoadedModels", false);
    isShowMaterialsWindow_ = EditorSettings::GetBool("sceneEditor.showMaterials", false);
    isShowLoadedSoundsWindow_ = EditorSettings::GetBool("sceneEditor.showLoadedSounds", false);
    isShowPlayingSoundsWindow_ = EditorSettings::GetBool("sceneEditor.showPlayingSounds", false);
    isShowLoggerWindow_ = EditorSettings::GetBool("sceneEditor.showLogger", true);
    isShowInputStateWindow_ = EditorSettings::GetBool("sceneEditor.showInputState", false);
    isShowInputCommandEditorWindow_ = EditorSettings::GetBool("sceneEditor.showInputCommandEditor", false);
}

SceneEditor::~SceneEditor() = default;

void SceneEditor::ShowImGui() {
    ShowMainWindow();
    HandleShortcuts();

    if (isShowHierarchy_) objectHierarchy_->ShowImGui();
    if (isShowObjectInspector_) objectInspector_->ShowImGui();
    if (isShowComponentInspector_) componentInspector_->ShowImGui();
    if (isShowVariablesMenu_) variablesMenu_->ShowImGui();
    if (isShowSceneView_) sceneView_->ShowImGui(objectHierarchy_->GetSelectedObjects(), commands_.get());
    if (isShowAssets_) assetsWindow_->ShowImGui();

    //--------- デバッグ用ウィンドウ（旧ImGuiManagerから移設） ---------//
    if (isShowLoadedTexturesWindow_) TextureManager::ShowImGuiLoadedTexturesWindow();
    if (isShowLoadedModelsWindow_) ModelManager::ShowImGuiLoadedModelsWindow();
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
        if (ImGui::BeginMenu("File")) {
            if (ImGui::MenuItem("New Scene...")) {
                isNewSceneRequested_ = true;
                newSceneName_ = "New Scene";
            }
            if (ImGui::MenuItem("Save Scene...", "Ctrl+S")) {
                saver_->Open();
            }
            if (ImGui::MenuItem("Load Scene...")) {
                loader_->Open();
            }
            ImGui::EndMenu();
        }
        if (ImGui::BeginMenu("Edit")) {
            const std::string undoLabel = commands_->CanUndo() ? ("Undo " + commands_->GetUndoName()) : "Undo";
            const std::string redoLabel = commands_->CanRedo() ? ("Redo " + commands_->GetRedoName()) : "Redo";
            if (ImGui::MenuItem(undoLabel.c_str(), "Ctrl+Z", false, commands_->CanUndo())) {
                PerformUndo();
            }
            if (ImGui::MenuItem(redoLabel.c_str(), "Ctrl+Y", false, commands_->CanRedo())) {
                PerformRedo();
            }
            ImGui::EndMenu();
        }
        if (ImGui::BeginMenu("Window")) {
            if (ImGui::MenuItem("Scene View", nullptr, &isShowSceneView_)) {
                EditorSettings::SetBool("sceneEditor.showSceneView", isShowSceneView_);
            }
            if (ImGui::MenuItem("Object Hierarchy", nullptr, &isShowHierarchy_)) {
                EditorSettings::SetBool("sceneEditor.showHierarchy", isShowHierarchy_);
            }
            if (ImGui::MenuItem("Object Inspector", nullptr, &isShowObjectInspector_)) {
                EditorSettings::SetBool("sceneEditor.showObjectInspector", isShowObjectInspector_);
            }
            if (ImGui::MenuItem("Scene Component Inspector", nullptr, &isShowComponentInspector_)) {
                EditorSettings::SetBool("sceneEditor.showComponentInspector", isShowComponentInspector_);
            }
            if (ImGui::MenuItem("Scene Variables", nullptr, &isShowVariablesMenu_)) {
                EditorSettings::SetBool("sceneEditor.showVariablesMenu", isShowVariablesMenu_);
            }
            if (ImGui::MenuItem("Assets", nullptr, &isShowAssets_)) {
                EditorSettings::SetBool("sceneEditor.showAssets", isShowAssets_);
            }
            ImGui::EndMenu();
        }
        if (ImGui::BeginMenu("Debug Windows")) {
            if (ImGui::MenuItem("Loaded Textures", nullptr, &isShowLoadedTexturesWindow_)) {
                EditorSettings::SetBool("sceneEditor.showLoadedTextures", isShowLoadedTexturesWindow_);
            }
            if (ImGui::MenuItem("Loaded Models", nullptr, &isShowLoadedModelsWindow_)) {
                EditorSettings::SetBool("sceneEditor.showLoadedModels", isShowLoadedModelsWindow_);
            }
            if (ImGui::MenuItem("Materials", nullptr, &isShowMaterialsWindow_)) {
                EditorSettings::SetBool("sceneEditor.showMaterials", isShowMaterialsWindow_);
            }
            if (ImGui::MenuItem("Loaded Sounds", nullptr, &isShowLoadedSoundsWindow_)) {
                EditorSettings::SetBool("sceneEditor.showLoadedSounds", isShowLoadedSoundsWindow_);
            }
            if (ImGui::MenuItem("Playing Sounds", nullptr, &isShowPlayingSoundsWindow_)) {
                EditorSettings::SetBool("sceneEditor.showPlayingSounds", isShowPlayingSoundsWindow_);
            }
            if (ImGui::MenuItem("Logger", nullptr, &isShowLoggerWindow_)) {
                EditorSettings::SetBool("sceneEditor.showLogger", isShowLoggerWindow_);
            }
            ImGui::Separator();
            if (ImGui::MenuItem("Input State", nullptr, &isShowInputStateWindow_)) {
                EditorSettings::SetBool("sceneEditor.showInputState", isShowInputStateWindow_);
            }
            if (ImGui::MenuItem("Input Command Editor", nullptr, &isShowInputCommandEditorWindow_)) {
                EditorSettings::SetBool("sceneEditor.showInputCommandEditor", isShowInputCommandEditorWindow_);
            }
            ImGui::Separator();
            ImGui::MenuItem("ImGui Demo Window", nullptr, &isShowImGuiDemoWindow_);
            ImGui::EndMenu();
        }
        ImGui::EndMainMenuBar();
    }

    if (!ImGui::Begin("Scene Editor")) {
        ImGui::End();
        return;
    }

    //--------- シーン名の編集 ---------//
    std::string sceneName = context_->GetName();
    if (ImGui::InputText("Scene Name", &sceneName, ImGuiInputTextFlags_EnterReturnsTrue)) {
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

    ImGui::End();

    //--------- 操作履歴 ---------//
    ImGui::Begin("History");
    commands_->ShowHistoryImGui();
    ImGui::End();
}

void SceneEditor::ShowPlayControls() {
    ImGui::Separator();
    if (!context_->IsPlaying()) {
        if (ImGui::Button("Play")) {
            context_->PlayStart();
            // 再生開始でオブジェクトのポインタ等が変わりうるため選択をクリアする
            objectHierarchy_->ClearSelection();
        }
    } else {
        if (ImGui::Button("Stop")) {
            context_->PlayStop();
            objectHierarchy_->ClearSelection();
        }
        ImGui::SameLine();
        if (context_->IsPaused()) {
            if (ImGui::Button("Resume")) {
                context_->PlayResume();
            }
        } else {
            if (ImGui::Button("Pause")) {
                context_->PlayPause();
            }
        }
        ImGui::SameLine();
        ImGui::BeginDisabled(!context_->IsPaused());
        if (ImGui::Button("Step Frame")) {
            context_->RequestStepFrame();
        }
        ImGui::EndDisabled();
    }
}

bool SceneEditor::ShowNewSceneModal() {
    bool created = false;
    if (isNewSceneRequested_) {
        ImGui::OpenPopup("New Scene");
        isNewSceneRequested_ = false;
    }
    if (ImGui::BeginPopupModal("New Scene", nullptr, ImGuiWindowFlags_AlwaysAutoResize)) {
        ImGui::TextColored(ImVec4(1.0f, 0.7f, 0.3f, 1.0f), "Current scene will be discarded (unsaved changes will be lost).");
        ImGui::InputText("Scene Name", &newSceneName_);
        if (ImGui::Button("Create", ImVec2(120, 0))) {
            context_->ClearSceneObjects();
            context_->ClearSceneComponents();
            context_->SetName(newSceneName_.empty() ? "New Scene" : newSceneName_);
            created = true;
            ImGui::CloseCurrentPopup();
        }
        ImGui::SameLine();
        if (ImGui::Button("Cancel", ImVec2(120, 0))) {
            ImGui::CloseCurrentPopup();
        }
        ImGui::EndPopup();
    }
    return created;
}

void SceneEditor::HandleShortcuts() {
    if (ImGui::GetIO().WantTextInput) return;
    if (ImGui::IsKeyChordPressed(ImGuiMod_Ctrl | ImGuiKey_Z)) {
        PerformUndo();
    }
    if (ImGui::IsKeyChordPressed(ImGuiMod_Ctrl | ImGuiKey_Y)) {
        PerformRedo();
    }
    if (ImGui::IsKeyChordPressed(ImGuiMod_Ctrl | ImGuiKey_S)) {
        saver_->Open();
    }
}

void SceneEditor::PerformUndo() {
    commands_->Undo();
    // オブジェクトの削除/再生成でポインタが変わる可能性があるため選択をクリアする
    objectHierarchy_->ClearSelection();
}

void SceneEditor::PerformRedo() {
    commands_->Redo();
    objectHierarchy_->ClearSelection();
}

} // namespace KashipanEngine
#endif // USE_IMGUI
