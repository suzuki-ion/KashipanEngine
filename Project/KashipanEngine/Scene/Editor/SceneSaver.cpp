#include "SceneSaver.h"
#ifdef USE_IMGUI
#include <imgui.h>
#include "Debug/Logger.h"
#include "Scene/SceneFileIO.h"
#include "Utilities/FileIO.h"
#include "Utilities/Translation.h"

namespace KashipanEngine {

void SceneSaver::Open() {
    LogScope scope;
    isOpenRequested_ = true;
    filePath_ = "Assets/Scenes/" + context_->GetName() + ".scene";
}

void SceneSaver::ShowImGui() {
    LogScope scope;
    if (isOpenRequested_) {
        ImGui::OpenPopup(TranslationLabel("editor.savescene.title"));
        isOpenRequested_ = false;
    }
    if (ImGui::BeginPopupModal(TranslationLabel("editor.savescene.title"), nullptr, ImGuiWindowFlags_AlwaysAutoResize)) {
        ImGui::InputText(TranslationLabel("editor.common.path"), &filePath_);
        if (ImGui::Button(TranslationLabel("editor.common.save"), ImVec2(120, 0))) {
            if (!filePath_.empty()) {
                SaveSceneToPath(context_->SaveSceneToJSON(), filePath_);
                ImGui::CloseCurrentPopup();
            }
        }
        ImGui::SameLine();
        if (ImGui::Button(TranslationLabel("editor.common.cancel"), ImVec2(120, 0))) {
            ImGui::CloseCurrentPopup();
        }
        ImGui::EndPopup();
    }
}

} // namespace KashipanEngine

#endif // USE_IMGUI
