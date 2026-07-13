#include "SceneSaver.h"
#ifdef USE_IMGUI
#include <imgui.h>
#include "Utilities/FileIO.h"

namespace KashipanEngine {

void SceneSaver::Open() {
    isOpenRequested_ = true;
    filePath_ = "Assets/Scenes/" + context_->GetName() + ".json";
}

void SceneSaver::ShowImGui() {
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

} // namespace KashipanEngine

#endif // USE_IMGUI
