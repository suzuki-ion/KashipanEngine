#include "EditorPreferences.h"
#ifdef USE_IMGUI
#include <imgui.h>
#include <imgui_stdlib.h>
#include <vector>

#include "Core/ProjectPaths.h"
#include "Core/UserSettings.h"
#include "Utilities/ImGuiCustom.h"
#include "Utilities/Translation.h"

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
    fontFiles_ = ProjectPaths::ListAssetFiles({ ".ttf", ".otf" });
    hasScannedFontFiles_ = true;
}

void EditorPreferences::EnsureDefaultPresets() {
    hasEnsuredDefaultPresets_ = true;

    JSON presets = UserSettings::GetJSON("editorUI.presets", JSON());
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
    if (!UserSettings::GetBool("editorUI.presets.unityAdded", false)) {
        presets["Unity"] = ColorsToJSON(BuildUnityStyle().Colors);
        UserSettings::SetBool("editorUI.presets.unityAdded", true);
        changed = true;
    }

    if (changed) {
        UserSettings::SetJSON("editorUI.presets", presets);
    }
}

void EditorPreferences::ShowImGui() {
    if (!ImGui::Begin(TranslationLabel("editor.preferences.window"))) {
        ImGui::End();
        return;
    }

    ImGui::TextDisabled("%s", TranslationC("editor.preferences.description"));

    //--------- 表示倍率 ---------//
    ImGui::SeparatorText(TranslationLabel("editor.preferences.displayscale"));
    float fontScale = UserSettings::GetFloat("editorUI.fontScale", 1.0f);
    if (ImGui::SliderFloat(TranslationLabel("editor.preferences.textscale"), &fontScale, 0.5f, 3.0f, "%.2f")) {
        UserSettings::SetFloat("editorUI.fontScale", fontScale);
    }
    ImGui::SetItemTooltip("%s", TranslationC("editor.preferences.textscale.tooltip"));
    float uiScale = UserSettings::GetFloat("editorUI.uiScale", 1.0f);
    if (ImGui::SliderFloat(TranslationLabel("editor.preferences.uiscale"), &uiScale, 0.5f, 3.0f, "%.2f")) {
        UserSettings::SetFloat("editorUI.uiScale", uiScale);
    }
    ImGui::SetItemTooltip("%s", TranslationC("editor.preferences.uiscale.tooltip"));

    //--------- フォント ---------//
    ImGui::SeparatorText(TranslationLabel("editor.preferences.font"));
    // Assetsの再帰スキャンは重いため、パネルを開いた直後の1回だけ行い、以後はキャッシュを使う
    if (!hasScannedFontFiles_) {
        RefreshFontFileList();
    }
    std::string fontPath = UserSettings::GetString("editorUI.fontPath", "");
    if (ImGuiCustom::SelectString(TranslationLabel("editor.preferences.fontfile"), fontPath, fontFiles_, true)) {
        UserSettings::SetString("editorUI.fontPath", fontPath);
    }
    ImGui::SetItemTooltip("%s", TranslationC("editor.preferences.fontfile.tooltip"));
    ImGui::SameLine();
    if (ImGui::SmallButton(TranslationLabel("editor.common.refresh"))) {
        RefreshFontFileList();
    }
    ImGui::SetItemTooltip("%s", TranslationC("editor.preferences.font.refresh.tooltip"));

    //--------- 配色プリセット ---------//
    ImGui::SeparatorText(TranslationLabel("editor.preferences.colors"));
    // 初回のみDark/Light/Classicの既定プリセットを登録する（ユーザーが削除した場合は復活させない）
    if (!hasEnsuredDefaultPresets_) {
        EnsureDefaultPresets();
    }
    const JSON presets = UserSettings::GetJSON("editorUI.presets", JSON::object());

    ImGui::TextDisabled("%s", TranslationC("editor.preferences.presets.description"));
    if (ImGui::BeginTable("##ColorPresets", 2, ImGuiTableFlags_SizingFixedFit)) {
        for (auto it = presets.begin(); it != presets.end(); ++it) {
            const bool isValidPreset = it.value().is_array() && it.value().size() == ImGuiCol_COUNT;
            ImGui::PushID(it.key().c_str());
            ImGui::TableNextRow();
            ImGui::TableNextColumn();
            ImGui::BeginDisabled(!isValidPreset);
            if (ImGui::Button(it.key().c_str())) {
                UserSettings::SetJSON("editorUI.colors", it.value());
            }
            ImGui::EndDisabled();
            ImGui::TableNextColumn();
            if (ImGui::SmallButton(TranslationLabel("editor.common.delete"))) {
                JSON updated = presets;
                updated.erase(it.key());
                UserSettings::SetJSON("editorUI.presets", updated);
            }
            ImGui::PopID();
        }
        ImGui::EndTable();
    }

    ImGui::SetNextItemWidth(200.0f);
    ImGui::InputTextWithHint("##NewPresetName", TranslationC("editor.preferences.presets.name.hint"), &newPresetNameBuffer_);
    ImGui::SameLine();
    ImGui::BeginDisabled(newPresetNameBuffer_.empty());
    if (ImGui::Button(TranslationLabel("editor.preferences.presets.save"))) {
        JSON updated = presets;
        updated[newPresetNameBuffer_] = ColorsToJSON(ImGui::GetStyle().Colors);
        UserSettings::SetJSON("editorUI.presets", updated);
        newPresetNameBuffer_.clear();
    }
    ImGui::EndDisabled();
    ImGui::SetItemTooltip("%s", TranslationC("editor.preferences.presets.save.tooltip"));

    if (ImGui::TreeNode(TranslationLabel("editor.preferences.customcolors"))) {
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
            UserSettings::SetJSON("editorUI.colors", ColorsToJSON(workingColors));
        }
        ImGui::TreePop();
    }

    ImGui::Spacing();
    if (ImGui::Button(TranslationLabel("editor.preferences.resetall"))) {
        UserSettings::SetFloat("editorUI.fontScale", 1.0f);
        UserSettings::SetFloat("editorUI.uiScale", 1.0f);
        UserSettings::SetString("editorUI.fontPath", "");
        ImGuiStyle temp;
        ImGui::StyleColorsDark(&temp);
        UserSettings::SetJSON("editorUI.colors", ColorsToJSON(temp.Colors));
    }

    ImGui::End();
}

} // namespace KashipanEngine
#endif // USE_IMGUI
