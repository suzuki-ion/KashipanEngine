#include "SceneListEditor.h"
#ifdef USE_IMGUI
#include <imgui.h>
#include <imgui_stdlib.h>

#include "Scene/SceneEditorContext.h"
#include "Scene/SceneManager.h"

namespace KashipanEngine {

void SceneListEditor::ShowImGui() {
    if (!ImGui::Begin("Scene List")) {
        ImGui::End();
        return;
    }

    auto *sceneManager = context_ ? context_->GetSceneManager() : nullptr;
    if (!sceneManager) {
        ImGui::TextDisabled("SceneManager is not available.");
        ImGui::End();
        return;
    }

    //--------- スタートアップシーンの指定 ---------//
    const std::string &startupName = sceneManager->GetStartupSceneName();
    if (ImGui::BeginCombo("Startup Scene", startupName.empty() ? "(None)" : startupName.c_str())) {
        if (ImGui::Selectable("(None)", startupName.empty())) {
            sceneManager->SetStartupSceneName("");
            sceneManager->SaveSceneList();
        }
        for (const auto &entry : sceneManager->GetRegisteredScenes()) {
            if (ImGui::Selectable(entry.name.c_str(), entry.name == startupName)) {
                sceneManager->SetStartupSceneName(entry.name);
                sceneManager->SaveSceneList();
            }
        }
        ImGui::EndCombo();
    }
    ImGui::SetItemTooltip("The scene loaded automatically at startup.");
    ImGui::Separator();

    //--------- 登録シーンの一覧・編集・切り替え ---------//
    // 行内のボタンで登録内容が変更されるため、スナップショットのコピーに対して走査する
    const std::vector<SceneManager::SceneEntry> entries = sceneManager->GetRegisteredScenes();
    const bool isPlaying = context_->IsPlaying();

    if (ImGui::BeginTable("##SceneListTable", 4,
        ImGuiTableFlags_Borders | ImGuiTableFlags_RowBg | ImGuiTableFlags_SizingStretchProp)) {
        ImGui::TableSetupColumn("Name", ImGuiTableColumnFlags_WidthStretch, 0.3f);
        ImGui::TableSetupColumn("File Path", ImGuiTableColumnFlags_WidthStretch, 0.5f);
        ImGui::TableSetupColumn("Switch", ImGuiTableColumnFlags_WidthFixed);
        ImGui::TableSetupColumn("Delete", ImGuiTableColumnFlags_WidthFixed);
        ImGui::TableHeadersRow();

        int rowIndex = 0;
        for (const auto &entry : entries) {
            ImGui::PushID(rowIndex++);
            ImGui::TableNextRow();

            //--------- シーン名の編集 ---------//
            ImGui::TableNextColumn();
            ImGui::SetNextItemWidth(-FLT_MIN);
            std::string name = entry.name;
            if (ImGui::InputText("##name", &name, ImGuiInputTextFlags_EnterReturnsTrue)) {
                if (sceneManager->RenameRegisteredScene(entry.name, name)) {
                    sceneManager->SaveSceneList();
                }
            }

            //--------- 読み込みJSONファイルパスの編集 ---------//
            ImGui::TableNextColumn();
            ImGui::SetNextItemWidth(-FLT_MIN);
            std::string filePath = entry.filePath;
            if (ImGui::InputText("##filePath", &filePath, ImGuiInputTextFlags_EnterReturnsTrue)) {
                if (sceneManager->SetRegisteredSceneFilePath(entry.name, filePath)) {
                    sceneManager->SaveSceneList();
                }
            }
            if (entry.filePath.empty()) {
                ImGui::SetItemTooltip("No file path: switching creates an empty scene\n(or uses the JSON registered from code).");
            }

            //--------- シーン切り替え ---------//
            ImGui::TableNextColumn();
            ImGui::BeginDisabled(isPlaying);
            if (ImGui::Button("Switch")) {
                sceneManager->ChangeScene(entry.name);
            }
            ImGui::EndDisabled();
            if (isPlaying) {
                ImGui::SetItemTooltip("Stop playing before switching scenes.");
            } else {
                ImGui::SetItemTooltip("Discard the current scene and switch to this scene.\n(Applied at the end of this frame)");
            }

            //--------- 登録の削除 ---------//
            ImGui::TableNextColumn();
            if (ImGui::Button("Delete")) {
                if (sceneManager->UnregisterScene(entry.name)) {
                    sceneManager->SaveSceneList();
                }
            }
            ImGui::SetItemTooltip("Remove this scene from the list.\n(The scene JSON file itself is not deleted)");

            ImGui::PopID();
        }
        ImGui::EndTable();
    }

    //--------- シーンの新規登録 ---------//
    ImGui::SeparatorText("Register New Scene");
    ImGui::InputText("Name##new", &newSceneName_);
    ImGui::InputText("File Path##new", &newSceneFilePath_);
    ImGui::BeginDisabled(newSceneName_.empty());
    if (ImGui::Button("Register")) {
        if (sceneManager->RegisterSceneFile(newSceneName_, newSceneFilePath_)) {
            sceneManager->SaveSceneList();
            newSceneName_.clear();
            newSceneFilePath_.clear();
        }
    }
    ImGui::EndDisabled();

    ImGui::End();
}

} // namespace KashipanEngine
#endif // USE_IMGUI
