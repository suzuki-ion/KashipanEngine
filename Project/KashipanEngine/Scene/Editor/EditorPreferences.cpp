#include "EditorPreferences.h"
#ifdef USE_IMGUI
#include <imgui.h>
#include <imgui_stdlib.h>
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

/// @brief Unity Editor（Darkスキン）に近い配色を組み立てる。ピクセル単位の再現ではなく近似
ImGuiStyle BuildUnityStyle() {
    ImGuiStyle style;
    ImGui::StyleColorsDark(&style);
    ImVec4 *c = style.Colors;

    const ImVec4 bgDarkest(0.145f, 0.145f, 0.149f, 1.00f); // メニューバー・スクロールバー背景
    const ImVec4 bgDark(0.176f, 0.176f, 0.188f, 1.00f);    // メインウィンドウ背景
    const ImVec4 bgMid(0.220f, 0.220f, 0.220f, 1.00f);     // パネル・タブ非アクティブ
    const ImVec4 bgLight(0.275f, 0.275f, 0.275f, 1.00f);   // ボタン・フレーム
    const ImVec4 bgLighter(0.333f, 0.333f, 0.333f, 1.00f); // ホバー時
    const ImVec4 frameBg(0.157f, 0.157f, 0.157f, 1.00f);   // 入力欄（へこんだ見た目）
    const ImVec4 accent(0.247f, 0.451f, 0.671f, 1.00f);        // Unity選択ハイライトの青
    const ImVec4 accentHover(0.306f, 0.510f, 0.729f, 1.00f);
    const ImVec4 accentActive(0.188f, 0.392f, 0.612f, 1.00f);
    const ImVec4 text(0.824f, 0.824f, 0.824f, 1.00f);
    const ImVec4 textDisabled(0.549f, 0.549f, 0.549f, 1.00f);
    const ImVec4 border(0.098f, 0.098f, 0.098f, 1.00f);

    c[ImGuiCol_Text] = text;
    c[ImGuiCol_TextDisabled] = textDisabled;
    c[ImGuiCol_WindowBg] = bgDark;
    c[ImGuiCol_ChildBg] = bgDark;
    c[ImGuiCol_PopupBg] = bgDark;
    c[ImGuiCol_Border] = border;
    c[ImGuiCol_BorderShadow] = ImVec4(0, 0, 0, 0);
    c[ImGuiCol_FrameBg] = frameBg;
    c[ImGuiCol_FrameBgHovered] = bgLight;
    c[ImGuiCol_FrameBgActive] = bgLighter;
    c[ImGuiCol_TitleBg] = bgDarkest;
    c[ImGuiCol_TitleBgActive] = bgMid;
    c[ImGuiCol_TitleBgCollapsed] = bgDarkest;
    c[ImGuiCol_MenuBarBg] = bgDarkest;
    c[ImGuiCol_ScrollbarBg] = bgDarkest;
    c[ImGuiCol_ScrollbarGrab] = bgLight;
    c[ImGuiCol_ScrollbarGrabHovered] = bgLighter;
    c[ImGuiCol_ScrollbarGrabActive] = accent;
    c[ImGuiCol_CheckMark] = accent;
    c[ImGuiCol_SliderGrab] = accent;
    c[ImGuiCol_SliderGrabActive] = accentActive;
    c[ImGuiCol_Button] = bgLight;
    c[ImGuiCol_ButtonHovered] = bgLighter;
    c[ImGuiCol_ButtonActive] = accentActive;
    c[ImGuiCol_Header] = accent;
    c[ImGuiCol_HeaderHovered] = accentHover;
    c[ImGuiCol_HeaderActive] = accentActive;
    c[ImGuiCol_Separator] = border;
    c[ImGuiCol_SeparatorHovered] = accentHover;
    c[ImGuiCol_SeparatorActive] = accentActive;
    c[ImGuiCol_ResizeGrip] = bgLight;
    c[ImGuiCol_ResizeGripHovered] = accentHover;
    c[ImGuiCol_ResizeGripActive] = accentActive;
    c[ImGuiCol_Tab] = bgMid;
    c[ImGuiCol_TabHovered] = accentHover;
    c[ImGuiCol_TabSelected] = accent;
    c[ImGuiCol_TabDimmed] = bgDarkest;
    c[ImGuiCol_TabDimmedSelected] = bgMid;
    c[ImGuiCol_DockingPreview] = accent;
    c[ImGuiCol_DockingEmptyBg] = bgDark;
    c[ImGuiCol_TextSelectedBg] = ImVec4(accent.x, accent.y, accent.z, 0.35f);
    c[ImGuiCol_DragDropTarget] = accentHover;
    c[ImGuiCol_NavCursor] = accent;
    c[ImGuiCol_NavWindowingHighlight] = accentHover;

    style.WindowRounding = 2.0f;
    style.FrameRounding = 2.0f;
    style.GrabRounding = 2.0f;
    style.TabRounding = 2.0f;
    return style;
}

} // namespace

void EditorPreferences::RefreshFontFileList() {
    fontFiles_.clear();
    CollectFilesRecursive(GetDirectoryDataByExtension("Assets", { ".ttf", ".otf" }, true, true), fontFiles_);
    hasScannedFontFiles_ = true;
}

void EditorPreferences::EnsureDefaultPresets() {
    hasEnsuredDefaultPresets_ = true;

    JSON presets = EditorSettings::GetJSON("editorUI.presets", JSON());
    if (!presets.is_object()) presets = JSON::object();
    bool changed = false;

    // Dark/Light/Classicは初回（プリセットが1つも無い場合）のみまとめて登録する。
    // ユーザーが意図的に全削除していた場合、再起動のたびに復活してしまうのを防ぐため
    if (presets.empty()) {
        ImGuiStyle temp;
        ImGui::StyleColorsDark(&temp);
        presets["Dark"] = ColorsToJSON(temp.Colors);
        ImGui::StyleColorsLight(&temp);
        presets["Light"] = ColorsToJSON(temp.Colors);
        ImGui::StyleColorsClassic(&temp);
        presets["Classic"] = ColorsToJSON(temp.Colors);
        changed = true;
    }

    // Unityプリセットは既定3種とは別に、1度だけ追加する（editorUI.presets自体が空でも
    // 個別に消されていても、二重追加やユーザー削除後の復活が起きないよう専用フラグで管理する）
    if (!EditorSettings::GetBool("editorUI.presets.unityAdded", false)) {
        presets["Unity"] = ColorsToJSON(BuildUnityStyle().Colors);
        EditorSettings::SetBool("editorUI.presets.unityAdded", true);
        changed = true;
    }

    if (changed) {
        EditorSettings::SetJSON("editorUI.presets", presets);
    }
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

    //--------- 配色プリセット ---------//
    ImGui::SeparatorText("Colors");
    // 初回のみDark/Light/Classicの既定プリセットを登録する（ユーザーが削除した場合は復活させない）
    if (!hasEnsuredDefaultPresets_) {
        EnsureDefaultPresets();
    }
    const JSON presets = EditorSettings::GetJSON("editorUI.presets", JSON::object());

    ImGui::TextDisabled("Presets (saved locally as JSON, not shared via Git):");
    if (ImGui::BeginTable("##ColorPresets", 2, ImGuiTableFlags_SizingFixedFit)) {
        for (auto it = presets.begin(); it != presets.end(); ++it) {
            const bool isValidPreset = it.value().is_array() && it.value().size() == ImGuiCol_COUNT;
            ImGui::PushID(it.key().c_str());
            ImGui::TableNextRow();
            ImGui::TableNextColumn();
            ImGui::BeginDisabled(!isValidPreset);
            if (ImGui::Button(it.key().c_str())) {
                EditorSettings::SetJSON("editorUI.colors", it.value());
            }
            ImGui::EndDisabled();
            ImGui::TableNextColumn();
            if (ImGui::SmallButton("Delete")) {
                JSON updated = presets;
                updated.erase(it.key());
                EditorSettings::SetJSON("editorUI.presets", updated);
            }
            ImGui::PopID();
        }
        ImGui::EndTable();
    }

    ImGui::SetNextItemWidth(200.0f);
    ImGui::InputTextWithHint("##NewPresetName", "New preset name", &newPresetNameBuffer_);
    ImGui::SameLine();
    ImGui::BeginDisabled(newPresetNameBuffer_.empty());
    if (ImGui::Button("Save Current Colors as Preset")) {
        JSON updated = presets;
        updated[newPresetNameBuffer_] = ColorsToJSON(ImGui::GetStyle().Colors);
        EditorSettings::SetJSON("editorUI.presets", updated);
        newPresetNameBuffer_.clear();
    }
    ImGui::EndDisabled();
    ImGui::SetItemTooltip("Save the currently active colors below as a new preset (e.g. a custom Unity-like theme).");

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
