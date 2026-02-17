#include "ScoreUI.h"
#include "Objects/Components/Player/ScoreManager.h"

namespace KashipanEngine {

#if defined(USE_IMGUI)
void ScoreUI::ShowImGui() {
    if (!scoreManager_) return;

    ImGui::SetNextWindowPos(ImVec2(10, 100), ImGuiCond_FirstUseEver);
    ImGui::SetNextWindowSize(ImVec2(250, 100), ImGuiCond_FirstUseEver);
    
    ImGui::Begin("Score", nullptr, ImGuiWindowFlags_NoCollapse);
    
    ImGui::PushFont(ImGui::GetIO().Fonts->Fonts[0]);
    ImGui::SetWindowFontScale(1.5f);
    ImGui::Text("SCORE: %d", scoreManager_->GetScore());
    ImGui::SetWindowFontScale(1.0f);
    ImGui::PopFont();
    
    ImGui::End();
}
#endif

} // namespace KashipanEngine
