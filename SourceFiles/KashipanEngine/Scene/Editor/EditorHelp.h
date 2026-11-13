#pragma once
#ifdef USE_IMGUI
#include <imgui.h>
namespace KashipanEngine {
inline void EditorHelp(const char *text, bool sameLine = true) {
    if (sameLine) ImGui::SameLine();
    ImGui::TextDisabled("(?)");
    // 実行中やオブジェクト未選択時など、無効化されたコントロールでもヘルプを表示できるようにする。
    if (ImGui::IsItemHovered(ImGuiHoveredFlags_AllowWhenDisabled)) {
        ImGui::BeginTooltip();
        ImGui::PushTextWrapPos(ImGui::GetFontSize() * 32.0f);
        ImGui::TextUnformatted(text);
        ImGui::PopTextWrapPos();
        ImGui::EndTooltip();
    }
}
}
#endif
