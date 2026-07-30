#include "SceneListEditor.h"
#ifdef USE_IMGUI
#include <imgui.h>
#include <imgui_stdlib.h>
#include <filesystem>

#include "Scene/SceneEditorContext.h"
#include "Scene/SceneFileIO.h"
#include "Scene/SceneManager.h"
#include "Utilities/FileIO.h"

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

    if (ImGui::BeginTable("##SceneListTable", 5,
        ImGuiTableFlags_Borders | ImGuiTableFlags_RowBg | ImGuiTableFlags_SizingStretchProp)) {
        ImGui::TableSetupColumn("Name", ImGuiTableColumnFlags_WidthStretch, 0.3f);
        ImGui::TableSetupColumn("File Path", ImGuiTableColumnFlags_WidthStretch, 0.5f);
        ImGui::TableSetupColumn("Switch", ImGuiTableColumnFlags_WidthFixed);
        ImGui::TableSetupColumn("Convert", ImGuiTableColumnFlags_WidthFixed);
        ImGui::TableSetupColumn("Delete", ImGuiTableColumnFlags_WidthFixed);
        ImGui::TableHeadersRow();

        for (const auto &entry : entries) {
            // 行のIDは整数(PushID(int))ではなく文字列(PushID(const char*))で積む。
            // TableHeadersRow側が列インデックスをPushID(int)で積んでおり、行インデックスと列インデックスが
            // 一致した場合に同じラベル文字列(Switch列/Delete列など)のIDが衝突していたための対策。
            ImGui::PushID(entry.name.c_str());
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

            //--------- フォルダ形式（.scene）への変換 ---------//
            ImGui::TableNextColumn();
            const bool canConvert = entry.filePath.size() > 5 && entry.filePath.substr(entry.filePath.size() - 5) == ".json";
            ImGui::BeginDisabled(!canConvert);
            if (ImGui::Button("Convert to Folder Format")) {
                ConvertSceneToFolderFormat(entry.name, entry.filePath);
            }
            ImGui::EndDisabled();
            if (canConvert) {
                ImGui::SetItemTooltip("Convert this scene's storage to 1-object-per-folder format\n(reduces Git conflicts in multi-developer work).");
            } else {
                ImGui::SetItemTooltip("Already in folder format, or no file path is set.");
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

    ShowConfirmDeleteOldFilePopup();

    ImGui::End();
}

void SceneListEditor::ConvertSceneToFolderFormat(const std::string &sceneName, const std::string &oldFilePath) {
    auto *sceneManager = context_ ? context_->GetSceneManager() : nullptr;
    if (!sceneManager) return;

    JSON sceneJson = LoadJSON(oldFilePath);
    if (sceneJson.empty()) return;

    // 拡張子(.json)を取り除いた同名フォルダへ .scene 形式で書き出す
    std::string newFolderPath = oldFilePath;
    if (newFolderPath.size() > 5 && newFolderPath.substr(newFolderPath.size() - 5) == ".json") {
        newFolderPath.erase(newFolderPath.size() - 5);
    }
    newFolderPath += ".scene";

    if (!SaveSceneToPath(sceneJson, newFolderPath)) return;
    if (sceneManager->SetRegisteredSceneFilePath(sceneName, newFolderPath)) {
        sceneManager->SaveSceneList();
    }

    // 元ファイルの削除は破壊的操作のため、変換自体は成功させた上で別途確認を挟む
    isConfirmDeleteOldFileRequested_ = true;
    pendingDeleteOldFilePath_ = oldFilePath;
}

void SceneListEditor::ShowConfirmDeleteOldFilePopup() {
    if (isConfirmDeleteOldFileRequested_) {
        ImGui::OpenPopup("Delete Original File?");
        isConfirmDeleteOldFileRequested_ = false;
    }
    if (ImGui::BeginPopupModal("Delete Original File?", nullptr, ImGuiWindowFlags_AlwaysAutoResize)) {
        ImGui::Text("Converted to folder format successfully.");
        ImGui::TextWrapped("Delete the original file?\n%s", pendingDeleteOldFilePath_.c_str());
        if (ImGui::Button("Delete", ImVec2(120, 0))) {
            std::error_code ec;
            std::filesystem::remove(pendingDeleteOldFilePath_, ec);
            pendingDeleteOldFilePath_.clear();
            ImGui::CloseCurrentPopup();
        }
        ImGui::SameLine();
        if (ImGui::Button("Keep", ImVec2(120, 0))) {
            pendingDeleteOldFilePath_.clear();
            ImGui::CloseCurrentPopup();
        }
        ImGui::EndPopup();
    }
}

} // namespace KashipanEngine
#endif // USE_IMGUI
