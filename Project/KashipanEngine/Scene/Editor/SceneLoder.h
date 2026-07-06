#pragma once
#ifdef USE_IMGUI
#include <imgui.h>
#include <filesystem>
#include <string>
#include <vector>
#include "Scene/SceneEditorContext.h"

namespace KashipanEngine {

class SceneEditor;

/// @brief シーンの読込メニュー（モーダルポップアップ）
class SceneLoader final {
public:
    SceneLoader(Passkey<SceneEditor>, SceneEditorContext *context) : context_(context) {}
    ~SceneLoader() = default;

    /// @brief 読込ポップアップを開く
    void Open() {
        isOpenRequested_ = true;
        RefreshFileList();
    }

    /// @brief ポップアップの描画（毎フレーム呼ぶ）
    /// @return シーンが読み込まれた場合は true（呼び出し側で選択状態や履歴のクリアを行う）
    bool ShowImGui() {
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
                if (!filePath_.empty() && std::filesystem::exists(filePath_)) {
                    JSON sceneJson = LoadJSON(filePath_);
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

private:
    /// @brief シーンファイル一覧を再取得する
    void RefreshFileList() {
        sceneFiles_.clear();
        static const char *kSearchFolders[] = {
            "Assets/Scenes",
            "Assets/KashipanEngine/LastSceneBackup",
        };
        for (const auto *folder : kSearchFolders) {
            std::error_code ec;
            if (!std::filesystem::exists(folder, ec)) continue;
            for (const auto &entry : std::filesystem::directory_iterator(folder, ec)) {
                if (!entry.is_regular_file()) continue;
                if (entry.path().extension() != ".json") continue;
                std::string path = entry.path().generic_string();
                sceneFiles_.push_back(std::move(path));
            }
        }
    }

    SceneEditorContext *context_ = nullptr;
    std::string filePath_;
    std::vector<std::string> sceneFiles_;
    bool isOpenRequested_ = false;
};

} // namespace KashipanEngine

#endif // USE_IMGUI
