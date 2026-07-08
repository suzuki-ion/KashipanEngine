#include "SceneEditor.h"
#ifdef USE_IMGUI
#include <imgui.h>
#include <string>

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
}

SceneEditor::~SceneEditor() = default;

void SceneEditor::ShowImGui() {
    ShowMainWindow();
    HandleShortcuts();

    if (isShowHierarchy_) objectHierarchy_->ShowImGui();
    if (isShowObjectInspector_) objectInspector_->ShowImGui();
    if (isShowComponentInspector_) componentInspector_->ShowImGui();
    if (isShowVariablesMenu_) variablesMenu_->ShowImGui();
    if (isShowSceneView_) sceneView_->ShowImGui(objectHierarchy_->GetSelectedObject(), commands_.get());
    if (isShowAssets_) assetsWindow_->ShowImGui();
}

void SceneEditor::ShowMainWindow() {
    if (!ImGui::Begin("Scene Editor", nullptr, ImGuiWindowFlags_MenuBar)) {
        ImGui::End();
        return;
    }

    //--------- メニューバー（保存・読込・Undo/Redo・ウィンドウ切替） ---------//
    if (ImGui::BeginMenuBar()) {
        if (ImGui::BeginMenu("File")) {
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
        ImGui::EndMenuBar();
    }

    //--------- シーン名の編集 ---------//
    std::string sceneName = context_->GetName();
    if (ImGui::InputText("Scene Name", &sceneName, ImGuiInputTextFlags_EnterReturnsTrue)) {
        context_->SetName(sceneName);
    }

    //--------- 再生制御（Unity風のPlay/Pause/Stop） ---------//
    ShowPlayControls();

    //--------- 保存・読込ポップアップ ---------//
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
