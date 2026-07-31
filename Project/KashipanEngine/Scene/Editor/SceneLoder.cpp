#include "SceneLoder.h"
#ifdef USE_IMGUI
#include <imgui.h>
#include <filesystem>
#include "Core/ProjectPaths.h"
#include "Scene/SceneBackupPath.h"
#include "Scene/SceneFileIO.h"
#include "Utilities/Conversion/ConvertString.h"
#include "Utilities/FileIO.h"
#include "Utilities/Translation.h"

namespace KashipanEngine {

void SceneLoader::Open() {
    isOpenRequested_ = true;
    RefreshFileList();
}

bool SceneLoader::ShowImGui() {
    bool loaded = false;
    if (isOpenRequested_) {
        ImGui::OpenPopup(TranslationLabel("editor.loadscene.title"));
        isOpenRequested_ = false;
    }
    if (ImGui::BeginPopupModal(TranslationLabel("editor.loadscene.title"), nullptr, ImGuiWindowFlags_AlwaysAutoResize)) {
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
        ImGui::InputText(TranslationLabel("editor.common.path"), &filePath_);

        if (ImGui::Button(TranslationLabel("editor.common.load"), ImVec2(120, 0))) {
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
        if (ImGui::Button(TranslationLabel("editor.common.cancel"), ImVec2(120, 0))) {
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
        // 検索対象は論理パスで書かれているため、走査する前に物理パスへ変換する
        const std::string physicalFolder = ProjectPaths::ToPhysical(folder);
        if (!std::filesystem::exists(physicalFolder, ec)) continue;
        for (const auto &entry : std::filesystem::directory_iterator(
                physicalFolder, std::filesystem::directory_options::skip_permission_denied, ec)) {
            // 単一ファイル形式（.json）とフォルダ形式（.scene）の両方を一覧に含める
            std::error_code statusError;
            const auto status = entry.status(statusError);
            if (statusError) continue;
            const bool isJsonFile = std::filesystem::is_regular_file(status) && entry.path().extension() == ".json";
            const bool isSceneFolder = std::filesystem::is_directory(status) && entry.path().extension() == ".scene";
            if (!isJsonFile && !isSceneFolder) continue;
            // 一覧はシーンの読み込みにそのまま渡すため、論理パスへ戻して保持する
            sceneFiles_.push_back(ProjectPaths::ToLogical(entry.path().generic_string()));
        }
    }
}

} // namespace KashipanEngine

#endif // USE_IMGUI
