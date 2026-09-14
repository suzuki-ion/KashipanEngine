#include "SceneSaver.h"
#ifdef USE_IMGUI
#include <imgui.h>
#include "Scene/SceneFileIO.h"
#include "Utilities/FileIO.h"
#include "Utilities/Translation.h"

namespace KashipanEngine {

void SceneSaver::Open() {
    isOpenRequested_ = true;
    filePath_ = "Assets/Scenes/" + context_->GetName() + ".scene";
}

void SceneSaver::ShowImGui() {
    if (isOpenRequested_) {
        ImGui::OpenPopup(TranslationLabel("editor.savescene.title"));
        isOpenRequested_ = false;
    }
    if (ImGui::BeginPopupModal(TranslationLabel("editor.savescene.title"), nullptr, ImGuiWindowFlags_AlwaysAutoResize)) {
        // 再生中はランタイムの変更が混ざった現在のシーン状態を保存できてしまわないよう、
        // 保存対象を再生開始前のスナップショットへ差し替える（保存操作自体は禁止しない）
        const bool isPlaying = context_->IsPlaying();
        if (isPlaying) {
            ImGui::TextColored(ImVec4(1.0f, 0.7f, 0.3f, 1.0f), "%s", TranslationC("editor.savescene.playing.warning"));
        }
        ImGui::InputText(TranslationLabel("editor.common.path"), &filePath_);
        if (ImGui::Button(TranslationLabel("editor.common.save"), ImVec2(120, 0))) {
            if (!filePath_.empty()) {
                SaveSceneToPath(isPlaying ? context_->GetEditModeSnapshot() : context_->SaveSceneToJSON(), filePath_);
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
