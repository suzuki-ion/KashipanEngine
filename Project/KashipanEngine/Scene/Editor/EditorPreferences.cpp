#include "EditorPreferences.h"
#ifdef USE_IMGUI
#include <imgui.h>
#include <vector>

#include "Scene/Editor/EditorSettings.h"
#include "Utilities/FileIO/Directory.h"
#include "Utilities/ImGuiCustom.h"

namespace KashipanEngine {

namespace {

/// @brief ImGuiStyleの配色をEditorSettingsへ保存するJSON配列（要素数ImGuiCol_COUNT、各要素は[r,g,b,a]）へ変換する
JSON ColorsToJSON(const ImVec4 *colors) {
    JSON arr = JSON::array();
    for (int i = 0; i < ImGuiCol_COUNT; ++i) {
        arr.push_back(JSON::array({ colors[i].x, colors[i].y, colors[i].z, colors[i].w }));
    }
    return arr;
}

/// @brief DirectoryDataは階層構造のままファイルを保持するため、フォント選択肢用に平坦化する
void CollectFilesRecursive(const DirectoryData &dir, std::vector<std::string> &out) {
    for (const auto &file : dir.files) out.push_back(file);
    for (const auto &subdir : dir.subdirectories) CollectFilesRecursive(subdir, out);
}

} // namespace

void EditorPreferences::RefreshFontFileList() {
    fontFiles_.clear();
    CollectFilesRecursive(GetDirectoryDataByExtension("Assets", { ".ttf", ".otf" }, true, true), fontFiles_);
    hasScannedFontFiles_ = true;
}

void EditorPreferences::ShowImGui() {
    if (!ImGui::Begin("Editor Preferences")) {
        ImGui::End();
        return;
    }

    ImGui::TextDisabled("These settings are personal and stored locally (not shared via Git).");

    //--------- 表示倍率 ---------//
    ImGui::SeparatorText("Display Scale");
    float fontScale = EditorSettings::GetFloat("editorUI.fontScale", 1.0f);
    if (ImGui::SliderFloat("Text Scale", &fontScale, 0.5f, 3.0f, "%.2f")) {
        EditorSettings::SetFloat("editorUI.fontScale", fontScale);
    }
    ImGui::SetItemTooltip("Scales rendered text size only.");
    float uiScale = EditorSettings::GetFloat("editorUI.uiScale", 1.0f);
    if (ImGui::SliderFloat("UI Scale", &uiScale, 0.5f, 3.0f, "%.2f")) {
        EditorSettings::SetFloat("editorUI.uiScale", uiScale);
    }
    ImGui::SetItemTooltip("Scales window padding, spacing, rounding, and other layout metrics.");

    //--------- フォント ---------//
    ImGui::SeparatorText("Font");
    // Assetsの再帰スキャンは重いため、パネルを開いた直後の1回だけ行い、以後はキャッシュを使う
    if (!hasScannedFontFiles_) {
        RefreshFontFileList();
    }
    std::string fontPath = EditorSettings::GetString("editorUI.fontPath", "");
    if (ImGuiCustom::SelectString("Font File", fontPath, fontFiles_, true)) {
        EditorSettings::SetString("editorUI.fontPath", fontPath);
    }
    ImGui::SetItemTooltip("(None) uses the current language's default font.");
    ImGui::SameLine();
    if (ImGui::SmallButton("Refresh")) {
        RefreshFontFileList();
    }
    ImGui::SetItemTooltip("Re-scan Assets for font files (e.g. after adding a new .ttf/.otf).");

    //--------- 配色 ---------//
    ImGui::SeparatorText("Colors");
    if (ImGui::Button("Dark")) {
        ImGuiStyle temp;
        ImGui::StyleColorsDark(&temp);
        EditorSettings::SetJSON("editorUI.colors", ColorsToJSON(temp.Colors));
    }
    ImGui::SameLine();
    if (ImGui::Button("Light")) {
        ImGuiStyle temp;
        ImGui::StyleColorsLight(&temp);
        EditorSettings::SetJSON("editorUI.colors", ColorsToJSON(temp.Colors));
    }
    ImGui::SameLine();
    if (ImGui::Button("Classic")) {
        ImGuiStyle temp;
        ImGui::StyleColorsClassic(&temp);
        EditorSettings::SetJSON("editorUI.colors", ColorsToJSON(temp.Colors));
    }

    if (ImGui::TreeNode("Custom Colors")) {
        // liveStyleは直前フレームにImGuiManagerが適用済みの現在の配色（初期値として使う）
        const ImGuiStyle &liveStyle = ImGui::GetStyle();
        ImVec4 workingColors[ImGuiCol_COUNT];
        for (int i = 0; i < ImGuiCol_COUNT; ++i) workingColors[i] = liveStyle.Colors[i];

        bool changed = false;
        for (int i = 0; i < ImGuiCol_COUNT; ++i) {
            ImGui::PushID(i);
            if (ImGui::ColorEdit4("##color", &workingColors[i].x, ImGuiColorEditFlags_AlphaBar)) {
                changed = true;
            }
            ImGui::SameLine();
            ImGui::TextUnformatted(ImGui::GetStyleColorName(i));
            ImGui::PopID();
        }
        // EditorSettings経由でしか反映されない（ImGuiManagerが次フレームでEditorSettingsから
        // 読み直してスタイルを再構築するため）、ここでliveStyleを直接書き換えても意味がない
        if (changed) {
            EditorSettings::SetJSON("editorUI.colors", ColorsToJSON(workingColors));
        }
        ImGui::TreePop();
    }

    ImGui::Spacing();
    if (ImGui::Button("Reset All to Default")) {
        EditorSettings::SetFloat("editorUI.fontScale", 1.0f);
        EditorSettings::SetFloat("editorUI.uiScale", 1.0f);
        EditorSettings::SetString("editorUI.fontPath", "");
        ImGuiStyle temp;
        ImGui::StyleColorsDark(&temp);
        EditorSettings::SetJSON("editorUI.colors", ColorsToJSON(temp.Colors));
    }

    ImGui::End();
}

} // namespace KashipanEngine
#endif // USE_IMGUI
