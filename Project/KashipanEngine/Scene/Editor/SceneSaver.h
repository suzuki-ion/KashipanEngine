#pragma once
#ifdef USE_IMGUI
#include <imgui.h>
#include <string>
#include "Scene/SceneEditorContext.h"

namespace KashipanEngine {

class SceneEditor;

/// @brief シーンの保存メニュー（モーダルポップアップ）
class SceneSaver final {
public:
    SceneSaver(Passkey<SceneEditor>, SceneEditorContext *context) : context_(context) {}
    ~SceneSaver() = default;

    /// @brief 保存ポップアップを開く
    void Open() {
        isOpenRequested_ = true;
        filePath_ = "Assets/Scenes/" + context_->GetName() + ".json";
    }

    /// @brief ポップアップの描画（毎フレーム呼ぶ）
    void ShowImGui() {
        if (isOpenRequested_) {
            ImGui::OpenPopup("Save Scene");
            isOpenRequested_ = false;
        }
        if (ImGui::BeginPopupModal("Save Scene", nullptr, ImGuiWindowFlags_AlwaysAutoResize)) {
            ImGui::InputText("Path", &filePath_);
            if (ImGui::Button("Save", ImVec2(120, 0))) {
                if (!filePath_.empty()) {
                    SaveJSON(context_->SaveSceneToJSON(), filePath_);
                    ImGui::CloseCurrentPopup();
                }
            }
            ImGui::SameLine();
            if (ImGui::Button("Cancel", ImVec2(120, 0))) {
                ImGui::CloseCurrentPopup();
            }
            ImGui::EndPopup();
        }
    }

private:
    SceneEditorContext *context_ = nullptr;
    std::string filePath_;
    bool isOpenRequested_ = false;
};

} // namespace KashipanEngine

#endif // USE_IMGUI
