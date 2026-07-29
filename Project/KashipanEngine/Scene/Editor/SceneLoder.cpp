#include "SceneLoder.h"
#ifdef USE_IMGUI
#include <imgui.h>
#include <filesystem>
#include "Scene/SceneBackupPath.h"
#include "Scene/SceneFileIO.h"
#include "Utilities/Conversion/ConvertString.h"
#include "Utilities/FileIO.h"

namespace KashipanEngine {

void SceneLoader::Open() {
    isOpenRequested_ = true;
    RefreshFileList();
}

bool SceneLoader::ShowImGui() {
    bool loaded = false;
    if (isOpenRequested_) {
        ImGui::OpenPopup("Load Scene");
        isOpenRequested_ = false;
    }
    if (ImGui::BeginPopupModal("Load Scene", nullptr, ImGuiWindowFlags_AlwaysAutoResize)) {
        // 検索フォルダ内のシーンファイル一覧から選択できるようにする
        if (ImGui::BeginListBox("##SceneFiles", ImVec2(400, 160))) {
            for (const auto &file : sceneFiles_) {
                const bool selected = (filePath_ == file);
                if (ImGui::Selectable(file.c_str(), selected)) {
                    filePath_ = file;
                }
            }
            ImGui::EndListBox();
        }
        ImGui::InputText("Path", &filePath_);

        if (ImGui::Button("Load", ImVec2(120, 0))) {
            std::error_code ec;
            if (!filePath_.empty() && std::filesystem::exists(Utf8StringToPath(filePath_), ec) && !ec) {
                JSON sceneJson = LoadSceneFromPath(filePath_);
                if (!sceneJson.empty() && context_->LoadSceneFromJSON(sceneJson)) {
                    loaded = true;
                }
                ImGui::CloseCurrentPopup();
            }
        }
        ImGui::SameLine();
        if (ImGui::Button("Cancel", ImVec2(120, 0))) {
            ImGui::CloseCurrentPopup();
        }
        ImGui::EndPopup();
    }
    return loaded;
}

void SceneLoader::RefreshFileList() {
    sceneFiles_.clear();
    static const char *kSearchFolders[] = {
        "Assets/Scenes",
        kSceneBackupDirectory,
    };
    for (const auto *folder : kSearchFolders) {
        std::error_code ec;
        if (!std::filesystem::exists(folder, ec)) continue;
        for (const auto &entry : std::filesystem::directory_iterator(
                folder, std::filesystem::directory_options::skip_permission_denied, ec)) {
            // 単一ファイル形式（.json）とフォルダ形式（.scene）の両方を一覧に含める
            std::error_code statusError;
            const auto status = entry.status(statusError);
            if (statusError) continue;
            const bool isJsonFile = std::filesystem::is_regular_file(status) && entry.path().extension() == ".json";
            const bool isSceneFolder = std::filesystem::is_directory(status) && entry.path().extension() == ".scene";
            if (!isJsonFile && !isSceneFolder) continue;
            std::string path = entry.path().generic_string();
            sceneFiles_.push_back(std::move(path));
        }
    }
}

} // namespace KashipanEngine

#endif // USE_IMGUI
