#include "EditorPreferences.h"
#ifdef USE_IMGUI
#include <imgui.h>
#include <imgui_stdlib.h>
#include <vector>

#include "Core/PlayerSettings.h"
#include "Core/ProjectPaths.h"
#include "Core/UserSettings.h"
#include "Debug/ImGuiManager.h"
#include "Debug/Logger.h"
#include "Scene/Editor/EditorKeyBindings.h"
#include "Scene/Editor/EditorWindowChrome.h"
#include "Utilities/ImGuiCustom.h"
#include "Utilities/Translation.h"

namespace KashipanEngine {

namespace {

/// @brief ImGuiStyleの配色をEditorSettingsへ保存するJSON配列（要素数ImGuiCol_COUNT、各要素は[r,g,b,a]）へ変換する
JSON ColorsToJSON(const ImVec4 *colors) {
    LogScope scope;
    JSON arr = JSON::array();
    for (int i = 0; i < ImGuiCol_COUNT; ++i) {
        arr.push_back(JSON::array({ colors[i].x, colors[i].y, colors[i].z, colors[i].w }));
    }
    return arr;
}

/// @brief Unity Editor（Darkスキン）に近い配色を組み立てる。ピクセル単位の再現ではなく近似
ImGuiStyle BuildUnityStyle() {
    LogScope scope;
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

/// @brief Misskeyの既定ダークテーマ「Mi Dark」に近い配色を組み立てる
ImGuiStyle BuildMisskeyMiDarkStyle() {
    LogScope scope;
    ImGuiStyle style;
    ImGui::StyleColorsDark(&style);
    ImVec4 *c = style.Colors;

    const ImVec4 bg(0.137f, 0.137f, 0.137f, 1.00f);          // 本家Mi Darkの bg: #232323
    const ImVec4 panel(0.176f, 0.176f, 0.176f, 1.00f);       // 本家Mi Darkの panel: #2d2d2d
    const ImVec4 panelHighlight(0.206f, 0.206f, 0.206f, 1.00f); // panelをlighten<3>
    const ImVec4 buttonBg(0.226f, 0.226f, 0.226f, 1.00f);    // panelをlighten<5>
    const ImVec4 buttonHoverBg(0.276f, 0.276f, 0.276f, 1.00f); // panelをlighten<10>
    const ImVec4 accent(0.525f, 0.702f, 0.000f, 1.00f);      // 本家の accent: #86b300
    const ImVec4 accentHover(0.616f, 0.792f, 0.078f, 1.00f);
    const ImVec4 accentActive(0.435f, 0.612f, 0.000f, 1.00f);
    const ImVec4 accentedBg(0.525f, 0.702f, 0.000f, 0.15f);  // 本家の accentedBg: accentをalpha<0.15>
    const ImVec4 fg(0.780f, 0.820f, 0.847f, 1.00f);          // 本家Mi Darkの fg: rgb(199,209,216)
    const ImVec4 divider(1.000f, 1.000f, 1.000f, 0.14f);     // 本家Mi Darkの divider: rgba(255,255,255,0.14)
    const ImVec4 error(0.925f, 0.255f, 0.216f, 1.00f);       // 本家の error: #ec4137

    c[ImGuiCol_Text] = fg;
    c[ImGuiCol_TextDisabled] = ImVec4(fg.x, fg.y, fg.z, 0.55f);
    c[ImGuiCol_WindowBg] = bg;
    c[ImGuiCol_ChildBg] = bg;
    c[ImGuiCol_PopupBg] = panelHighlight;
    c[ImGuiCol_Border] = divider;
    c[ImGuiCol_BorderShadow] = ImVec4(0, 0, 0, 0);
    c[ImGuiCol_FrameBg] = panel;
    c[ImGuiCol_FrameBgHovered] = panelHighlight;
    c[ImGuiCol_FrameBgActive] = buttonHoverBg;
    c[ImGuiCol_TitleBg] = bg;
    c[ImGuiCol_TitleBgActive] = panel;
    c[ImGuiCol_TitleBgCollapsed] = bg;
    c[ImGuiCol_MenuBarBg] = panel;
    c[ImGuiCol_ScrollbarBg] = bg;
    c[ImGuiCol_ScrollbarGrab] = buttonBg;
    c[ImGuiCol_ScrollbarGrabHovered] = buttonHoverBg;
    c[ImGuiCol_ScrollbarGrabActive] = accent;
    c[ImGuiCol_CheckMark] = accent;
    c[ImGuiCol_SliderGrab] = accent;
    c[ImGuiCol_SliderGrabActive] = accentActive;
    c[ImGuiCol_Button] = buttonBg;
    c[ImGuiCol_ButtonHovered] = buttonHoverBg;
    c[ImGuiCol_ButtonActive] = accentActive;
    c[ImGuiCol_Header] = accentedBg;
    c[ImGuiCol_HeaderHovered] = accentHover;
    c[ImGuiCol_HeaderActive] = accentActive;
    c[ImGuiCol_Separator] = divider;
    c[ImGuiCol_SeparatorHovered] = accentHover;
    c[ImGuiCol_SeparatorActive] = accentActive;
    c[ImGuiCol_ResizeGrip] = buttonBg;
    c[ImGuiCol_ResizeGripHovered] = accentHover;
    c[ImGuiCol_ResizeGripActive] = accentActive;
    c[ImGuiCol_Tab] = panel;
    c[ImGuiCol_TabHovered] = accentHover;
    c[ImGuiCol_TabSelected] = accent;
    c[ImGuiCol_TabDimmed] = bg;
    c[ImGuiCol_TabDimmedSelected] = panel;
    c[ImGuiCol_DockingPreview] = accentedBg;
    c[ImGuiCol_DockingEmptyBg] = bg;
    c[ImGuiCol_TextSelectedBg] = accentedBg;
    c[ImGuiCol_DragDropTarget] = accentHover;
    c[ImGuiCol_NavCursor] = accent;
    c[ImGuiCol_NavWindowingHighlight] = accentHover;
    c[ImGuiCol_PlotLinesHovered] = error;
    c[ImGuiCol_PlotHistogramHovered] = error;

    style.WindowRounding = 6.0f;
    style.FrameRounding = 6.0f;
    style.GrabRounding = 6.0f;
    style.TabRounding = 6.0f;
    style.PopupRounding = 6.0f;
    style.ChildRounding = 6.0f;
    return style;
}

/// @brief Misskeyの既定ライトテーマ「Mi Light」に近い配色を組み立てる
ImGuiStyle BuildMisskeyMiLightStyle() {
    LogScope scope;
    ImGuiStyle style;
    ImGui::StyleColorsLight(&style);
    ImVec4 *c = style.Colors;

    const ImVec4 bg(0.976f, 0.976f, 0.976f, 1.00f);          // 本家Mi Lightの bg: #f9f9f9
    const ImVec4 panel(1.000f, 1.000f, 1.000f, 1.00f);       // 本家Mi Lightの panel: #fff
    const ImVec4 panelHighlight(0.969f, 0.969f, 0.969f, 1.00f); // panelをdarken<3>
    const ImVec4 buttonBg(0.949f, 0.949f, 0.949f, 1.00f);    // panelをdarken<5>
    const ImVec4 buttonHoverBg(0.902f, 0.902f, 0.902f, 1.00f); // panelをdarken<10>
    const ImVec4 accent(0.525f, 0.702f, 0.000f, 1.00f);      // 本家の accent: #86b300
    const ImVec4 accentHover(0.616f, 0.792f, 0.078f, 1.00f);
    const ImVec4 accentActive(0.435f, 0.612f, 0.000f, 1.00f);
    const ImVec4 accentedBg(0.525f, 0.702f, 0.000f, 0.15f);  // 本家の accentedBg: accentをalpha<0.15>
    const ImVec4 fg(0.404f, 0.404f, 0.404f, 1.00f);          // 本家Mi Lightの fg: #676767
    const ImVec4 divider(0.910f, 0.910f, 0.910f, 1.00f);     // 本家Mi Lightの divider: #e8e8e8
    const ImVec4 error(0.925f, 0.255f, 0.216f, 1.00f);       // 本家の error: #ec4137

    c[ImGuiCol_Text] = fg;
    c[ImGuiCol_TextDisabled] = ImVec4(fg.x, fg.y, fg.z, 0.55f);
    c[ImGuiCol_WindowBg] = bg;
    c[ImGuiCol_ChildBg] = bg;
    c[ImGuiCol_PopupBg] = panelHighlight;
    c[ImGuiCol_Border] = divider;
    c[ImGuiCol_BorderShadow] = ImVec4(0, 0, 0, 0);
    c[ImGuiCol_FrameBg] = panel;
    c[ImGuiCol_FrameBgHovered] = panelHighlight;
    c[ImGuiCol_FrameBgActive] = buttonHoverBg;
    c[ImGuiCol_TitleBg] = bg;
    c[ImGuiCol_TitleBgActive] = panel;
    c[ImGuiCol_TitleBgCollapsed] = bg;
    c[ImGuiCol_MenuBarBg] = panel;
    c[ImGuiCol_ScrollbarBg] = bg;
    c[ImGuiCol_ScrollbarGrab] = buttonBg;
    c[ImGuiCol_ScrollbarGrabHovered] = buttonHoverBg;
    c[ImGuiCol_ScrollbarGrabActive] = accent;
    c[ImGuiCol_CheckMark] = accent;
    c[ImGuiCol_SliderGrab] = accent;
    c[ImGuiCol_SliderGrabActive] = accentActive;
    c[ImGuiCol_Button] = buttonBg;
    c[ImGuiCol_ButtonHovered] = buttonHoverBg;
    c[ImGuiCol_ButtonActive] = accentActive;
    c[ImGuiCol_Header] = accentedBg;
    c[ImGuiCol_HeaderHovered] = accentHover;
    c[ImGuiCol_HeaderActive] = accentActive;
    c[ImGuiCol_Separator] = divider;
    c[ImGuiCol_SeparatorHovered] = accentHover;
    c[ImGuiCol_SeparatorActive] = accentActive;
    c[ImGuiCol_ResizeGrip] = buttonBg;
    c[ImGuiCol_ResizeGripHovered] = accentHover;
    c[ImGuiCol_ResizeGripActive] = accentActive;
    c[ImGuiCol_Tab] = panel;
    c[ImGuiCol_TabHovered] = accentHover;
    c[ImGuiCol_TabSelected] = accent;
    c[ImGuiCol_TabDimmed] = bg;
    c[ImGuiCol_TabDimmedSelected] = panel;
    c[ImGuiCol_DockingPreview] = accentedBg;
    c[ImGuiCol_DockingEmptyBg] = bg;
    c[ImGuiCol_TextSelectedBg] = accentedBg;
    c[ImGuiCol_DragDropTarget] = accentHover;
    c[ImGuiCol_NavCursor] = accent;
    c[ImGuiCol_NavWindowingHighlight] = accentHover;
    c[ImGuiCol_PlotLinesHovered] = error;
    c[ImGuiCol_PlotHistogramHovered] = error;

    style.WindowRounding = 6.0f;
    style.FrameRounding = 6.0f;
    style.GrabRounding = 6.0f;
    style.TabRounding = 6.0f;
    style.PopupRounding = 6.0f;
    style.ChildRounding = 6.0f;
    return style;
}

/// @brief Discordの既定ダークテーマ「アッシュ（Ash）」に近い配色を組み立てる。
///        Discord公式クライアントのスタイルシートから採取された値を基にしている
///        （出典: https://css.gomuks.app/theme/discord-ash.css）
ImGuiStyle BuildDiscordAshStyle() {
    LogScope scope;
    ImGuiStyle style;
    ImGui::StyleColorsDark(&style);
    ImVec4 *c = style.Colors;

    const ImVec4 bg(0.196f, 0.200f, 0.224f, 1.00f);          // 本家の background-color: #323339
    const ImVec4 panel(0.173f, 0.176f, 0.196f, 1.00f);       // 本家の room-list-background: #2c2d32
    const ImVec4 panelHighlight(0.224f, 0.227f, 0.255f, 1.00f); // 本家の composer-background-color: #393a41
    const ImVec4 buttonBg(0.280f, 0.285f, 0.319f, 1.00f);    // panelHighlightをさらに明るく
    const ImVec4 buttonHoverBg(0.336f, 0.342f, 0.383f, 1.00f);
    const ImVec4 accent(0.345f, 0.396f, 0.949f, 1.00f);      // Discordブランドカラー(ブルプル): #5865F2
    const ImVec4 accentHover(0.494f, 0.533f, 0.961f, 1.00f);
    const ImVec4 accentActive(0.197f, 0.259f, 0.937f, 1.00f);
    const ImVec4 accentedBg(0.345f, 0.396f, 0.949f, 0.15f);  // accentをalpha<0.15>
    const ImVec4 fg(0.855f, 0.867f, 0.855f, 1.00f);          // 本家の text-color: #daddda
    const ImVec4 divider(0.592f, 0.592f, 0.624f, 0.24f);     // 本家の segment-divider相当
    const ImVec4 error(0.929f, 0.259f, 0.271f, 1.00f);       // 本家の discord-red: #ed4245

    c[ImGuiCol_Text] = fg;
    c[ImGuiCol_TextDisabled] = ImVec4(fg.x, fg.y, fg.z, 0.55f);
    c[ImGuiCol_WindowBg] = bg;
    c[ImGuiCol_ChildBg] = bg;
    c[ImGuiCol_PopupBg] = panelHighlight;
    c[ImGuiCol_Border] = divider;
    c[ImGuiCol_BorderShadow] = ImVec4(0, 0, 0, 0);
    c[ImGuiCol_FrameBg] = panelHighlight;
    c[ImGuiCol_FrameBgHovered] = buttonBg;
    c[ImGuiCol_FrameBgActive] = buttonHoverBg;
    c[ImGuiCol_TitleBg] = panel;
    c[ImGuiCol_TitleBgActive] = panel;
    c[ImGuiCol_TitleBgCollapsed] = panel;
    c[ImGuiCol_MenuBarBg] = panel;
    c[ImGuiCol_ScrollbarBg] = bg;
    c[ImGuiCol_ScrollbarGrab] = buttonBg;
    c[ImGuiCol_ScrollbarGrabHovered] = buttonHoverBg;
    c[ImGuiCol_ScrollbarGrabActive] = accent;
    c[ImGuiCol_CheckMark] = accent;
    c[ImGuiCol_SliderGrab] = accent;
    c[ImGuiCol_SliderGrabActive] = accentActive;
    c[ImGuiCol_Button] = buttonBg;
    c[ImGuiCol_ButtonHovered] = buttonHoverBg;
    c[ImGuiCol_ButtonActive] = accentActive;
    c[ImGuiCol_Header] = accentedBg;
    c[ImGuiCol_HeaderHovered] = accentHover;
    c[ImGuiCol_HeaderActive] = accentActive;
    c[ImGuiCol_Separator] = divider;
    c[ImGuiCol_SeparatorHovered] = accentHover;
    c[ImGuiCol_SeparatorActive] = accentActive;
    c[ImGuiCol_ResizeGrip] = buttonBg;
    c[ImGuiCol_ResizeGripHovered] = accentHover;
    c[ImGuiCol_ResizeGripActive] = accentActive;
    c[ImGuiCol_Tab] = panel;
    c[ImGuiCol_TabHovered] = accentHover;
    c[ImGuiCol_TabSelected] = accent;
    c[ImGuiCol_TabDimmed] = panel;
    c[ImGuiCol_TabDimmedSelected] = bg;
    c[ImGuiCol_DockingPreview] = accentedBg;
    c[ImGuiCol_DockingEmptyBg] = panel;
    c[ImGuiCol_TextSelectedBg] = accentedBg;
    c[ImGuiCol_DragDropTarget] = accentHover;
    c[ImGuiCol_NavCursor] = accent;
    c[ImGuiCol_NavWindowingHighlight] = accentHover;
    c[ImGuiCol_PlotLinesHovered] = error;
    c[ImGuiCol_PlotHistogramHovered] = error;

    style.WindowRounding = 8.0f;
    style.FrameRounding = 4.0f;
    style.GrabRounding = 4.0f;
    style.TabRounding = 4.0f;
    style.PopupRounding = 8.0f;
    style.ChildRounding = 8.0f;
    return style;
}

/// @brief Steam（PCゲーム配信プラットフォーム）クライアントのダークテーマに近い配色を組み立てる。
///        Steamの公式パレットとして広く知られる値を基にしている
///        （出典: https://colorswall.com/palette/193 ほか。#171a21/#1b2838/#2a475e/#66c0f4/#c7d5e0）
ImGuiStyle BuildSteamStyle() {
    LogScope scope;
    ImGuiStyle style;
    ImGui::StyleColorsDark(&style);
    ImVec4 *c = style.Colors;

    const ImVec4 bg(0.106f, 0.157f, 0.220f, 1.00f);          // メイン背景: #1b2838
    const ImVec4 panel(0.090f, 0.102f, 0.129f, 1.00f);       // 最も暗い部分（タイトルバー等）: #171a21
    const ImVec4 panelHighlight(0.165f, 0.278f, 0.369f, 1.00f); // カード・入力欄の背景: #2a475e
    const ImVec4 buttonBg(0.202f, 0.341f, 0.452f, 1.00f);    // panelHighlightを明るく
    const ImVec4 buttonHoverBg(0.239f, 0.404f, 0.535f, 1.00f);
    const ImVec4 accent(0.400f, 0.753f, 0.957f, 1.00f);      // Steamブルー（リンク・ハイライト）: #66c0f4
    const ImVec4 accentHover(0.549f, 0.814f, 0.968f, 1.00f);
    const ImVec4 accentActive(0.251f, 0.691f, 0.946f, 1.00f);
    const ImVec4 accentedBg(0.400f, 0.753f, 0.957f, 0.15f);  // accentをalpha<0.15>
    const ImVec4 fg(0.780f, 0.835f, 0.878f, 1.00f);          // 文字色: #c7d5e0
    const ImVec4 divider(1.000f, 1.000f, 1.000f, 0.10f);
    // Steamの公式パレットには明確なエラー/警告用の赤が無いため、雰囲気に合わせた近似値を使用
    const ImVec4 error(0.757f, 0.361f, 0.361f, 1.00f);

    c[ImGuiCol_Text] = fg;
    c[ImGuiCol_TextDisabled] = ImVec4(fg.x, fg.y, fg.z, 0.55f);
    c[ImGuiCol_WindowBg] = bg;
    c[ImGuiCol_ChildBg] = bg;
    c[ImGuiCol_PopupBg] = panelHighlight;
    c[ImGuiCol_Border] = divider;
    c[ImGuiCol_BorderShadow] = ImVec4(0, 0, 0, 0);
    c[ImGuiCol_FrameBg] = panelHighlight;
    c[ImGuiCol_FrameBgHovered] = buttonBg;
    c[ImGuiCol_FrameBgActive] = buttonHoverBg;
    c[ImGuiCol_TitleBg] = panel;
    c[ImGuiCol_TitleBgActive] = panel;
    c[ImGuiCol_TitleBgCollapsed] = panel;
    c[ImGuiCol_MenuBarBg] = panel;
    c[ImGuiCol_ScrollbarBg] = bg;
    c[ImGuiCol_ScrollbarGrab] = buttonBg;
    c[ImGuiCol_ScrollbarGrabHovered] = buttonHoverBg;
    c[ImGuiCol_ScrollbarGrabActive] = accent;
    c[ImGuiCol_CheckMark] = accent;
    c[ImGuiCol_SliderGrab] = accent;
    c[ImGuiCol_SliderGrabActive] = accentActive;
    c[ImGuiCol_Button] = buttonBg;
    c[ImGuiCol_ButtonHovered] = buttonHoverBg;
    c[ImGuiCol_ButtonActive] = accentActive;
    c[ImGuiCol_Header] = accentedBg;
    c[ImGuiCol_HeaderHovered] = accentHover;
    c[ImGuiCol_HeaderActive] = accentActive;
    c[ImGuiCol_Separator] = divider;
    c[ImGuiCol_SeparatorHovered] = accentHover;
    c[ImGuiCol_SeparatorActive] = accentActive;
    c[ImGuiCol_ResizeGrip] = buttonBg;
    c[ImGuiCol_ResizeGripHovered] = accentHover;
    c[ImGuiCol_ResizeGripActive] = accentActive;
    c[ImGuiCol_Tab] = panel;
    c[ImGuiCol_TabHovered] = accentHover;
    c[ImGuiCol_TabSelected] = accent;
    c[ImGuiCol_TabDimmed] = panel;
    c[ImGuiCol_TabDimmedSelected] = bg;
    c[ImGuiCol_DockingPreview] = accentedBg;
    c[ImGuiCol_DockingEmptyBg] = panel;
    c[ImGuiCol_TextSelectedBg] = accentedBg;
    c[ImGuiCol_DragDropTarget] = accentHover;
    c[ImGuiCol_NavCursor] = accent;
    c[ImGuiCol_NavWindowingHighlight] = accentHover;
    c[ImGuiCol_PlotLinesHovered] = error;
    c[ImGuiCol_PlotHistogramHovered] = error;

    // Steamクライアントはあまり丸みを帯びていない、比較的フラットなUIのため角丸は控えめにする
    style.WindowRounding = 2.0f;
    style.FrameRounding = 2.0f;
    style.GrabRounding = 2.0f;
    style.TabRounding = 2.0f;
    style.PopupRounding = 2.0f;
    style.ChildRounding = 2.0f;
    return style;
}

/// @brief GitHubのダークテーマに近い配色を組み立てる。デザインシステム「Primer」の色トークンを基にしている
///        （出典: https://primer.style/primitives/colors/ ほか。#0d1117/#161b22/#30363d/#e6edf3/#58a6ff）
ImGuiStyle BuildGitHubStyle() {
    LogScope scope;
    ImGuiStyle style;
    ImGui::StyleColorsDark(&style);
    ImVec4 *c = style.Colors;

    const ImVec4 bg(0.051f, 0.067f, 0.090f, 1.00f);          // canvas.default(dark): #0d1117
    const ImVec4 panel(0.086f, 0.106f, 0.133f, 1.00f);       // canvas.subtle(dark): #161b22
    const ImVec4 panelHighlight(0.118f, 0.145f, 0.180f, 1.00f); // panelを少し明るく
    const ImVec4 buttonBg(0.150f, 0.180f, 0.220f, 1.00f);
    const ImVec4 buttonHoverBg(0.188f, 0.212f, 0.239f, 1.00f); // border.default(dark): #30363d
    const ImVec4 accent(0.345f, 0.651f, 1.000f, 1.00f);      // accent.fg(dark): #58a6ff
    const ImVec4 accentHover(0.505f, 0.736f, 1.000f, 1.00f);
    const ImVec4 accentActive(0.185f, 0.566f, 1.000f, 1.00f);
    const ImVec4 accentedBg(0.345f, 0.651f, 1.000f, 0.15f);  // accentをalpha<0.15>
    const ImVec4 fg(0.902f, 0.929f, 0.953f, 1.00f);          // fg.default(dark): #e6edf3
    const ImVec4 divider(0.188f, 0.212f, 0.239f, 1.00f);     // border.default(dark): #30363d
    const ImVec4 error(0.973f, 0.318f, 0.286f, 1.00f);       // danger.fg(dark): #f85149

    c[ImGuiCol_Text] = fg;
    c[ImGuiCol_TextDisabled] = ImVec4(fg.x, fg.y, fg.z, 0.55f);
    c[ImGuiCol_WindowBg] = bg;
    c[ImGuiCol_ChildBg] = bg;
    c[ImGuiCol_PopupBg] = panelHighlight;
    c[ImGuiCol_Border] = divider;
    c[ImGuiCol_BorderShadow] = ImVec4(0, 0, 0, 0);
    c[ImGuiCol_FrameBg] = panel;
    c[ImGuiCol_FrameBgHovered] = panelHighlight;
    c[ImGuiCol_FrameBgActive] = buttonHoverBg;
    c[ImGuiCol_TitleBg] = bg;
    c[ImGuiCol_TitleBgActive] = panel;
    c[ImGuiCol_TitleBgCollapsed] = bg;
    c[ImGuiCol_MenuBarBg] = panel;
    c[ImGuiCol_ScrollbarBg] = bg;
    c[ImGuiCol_ScrollbarGrab] = buttonBg;
    c[ImGuiCol_ScrollbarGrabHovered] = buttonHoverBg;
    c[ImGuiCol_ScrollbarGrabActive] = accent;
    c[ImGuiCol_CheckMark] = accent;
    c[ImGuiCol_SliderGrab] = accent;
    c[ImGuiCol_SliderGrabActive] = accentActive;
    c[ImGuiCol_Button] = buttonBg;
    c[ImGuiCol_ButtonHovered] = buttonHoverBg;
    c[ImGuiCol_ButtonActive] = accentActive;
    c[ImGuiCol_Header] = accentedBg;
    c[ImGuiCol_HeaderHovered] = accentHover;
    c[ImGuiCol_HeaderActive] = accentActive;
    c[ImGuiCol_Separator] = divider;
    c[ImGuiCol_SeparatorHovered] = accentHover;
    c[ImGuiCol_SeparatorActive] = accentActive;
    c[ImGuiCol_ResizeGrip] = buttonBg;
    c[ImGuiCol_ResizeGripHovered] = accentHover;
    c[ImGuiCol_ResizeGripActive] = accentActive;
    c[ImGuiCol_Tab] = panel;
    c[ImGuiCol_TabHovered] = accentHover;
    c[ImGuiCol_TabSelected] = accent;
    c[ImGuiCol_TabDimmed] = bg;
    c[ImGuiCol_TabDimmedSelected] = panel;
    c[ImGuiCol_DockingPreview] = accentedBg;
    c[ImGuiCol_DockingEmptyBg] = bg;
    c[ImGuiCol_TextSelectedBg] = accentedBg;
    c[ImGuiCol_DragDropTarget] = accentHover;
    c[ImGuiCol_NavCursor] = accent;
    c[ImGuiCol_NavWindowingHighlight] = accentHover;
    c[ImGuiCol_PlotLinesHovered] = error;
    c[ImGuiCol_PlotHistogramHovered] = error;

    style.WindowRounding = 6.0f;
    style.FrameRounding = 6.0f;
    style.GrabRounding = 6.0f;
    style.TabRounding = 6.0f;
    style.PopupRounding = 6.0f;
    style.ChildRounding = 6.0f;
    return style;
}

/// @brief YouTubeのライトテーマに近い配色を組み立てる。ブランドカラーの赤をアクセントに使用
///        （出典: 各種配色まとめサイトで広く一致する値。#ffffff/#f2f2f2/#0f0f0f/#606060/#ff0000）
ImGuiStyle BuildYouTubeLightStyle() {
    LogScope scope;
    ImGuiStyle style;
    ImGui::StyleColorsLight(&style);
    ImVec4 *c = style.Colors;

    const ImVec4 bg(1.000f, 1.000f, 1.000f, 1.00f);          // 背景: #ffffff
    const ImVec4 panel(0.949f, 0.949f, 0.949f, 1.00f);       // チップ・パネル背景: #f2f2f2
    const ImVec4 panelHighlight(0.898f, 0.898f, 0.898f, 1.00f); // 区切り線と同色: #e5e5e5
    const ImVec4 buttonBg(0.949f, 0.949f, 0.949f, 1.00f);
    const ImVec4 buttonHoverBg(0.898f, 0.898f, 0.898f, 1.00f);
    const ImVec4 accent(1.000f, 0.000f, 0.000f, 1.00f);      // ブランドカラーの赤: #ff0000
    const ImVec4 accentHover(1.000f, 0.160f, 0.160f, 1.00f);
    const ImVec4 accentActive(0.840f, 0.000f, 0.000f, 1.00f);
    const ImVec4 accentedBg(1.000f, 0.000f, 0.000f, 0.12f);  // accentをalpha<0.12>
    const ImVec4 fg(0.059f, 0.059f, 0.059f, 1.00f);          // 文字色: #0f0f0f
    const ImVec4 divider(0.898f, 0.898f, 0.898f, 1.00f);     // #e5e5e5
    // 公式に明確なエラー色の定義は無いため、赤いアクセントとの区別のみを目的とした近似値を使用
    const ImVec4 error(0.906f, 0.298f, 0.235f, 1.00f);

    c[ImGuiCol_Text] = fg;
    c[ImGuiCol_TextDisabled] = ImVec4(fg.x, fg.y, fg.z, 0.55f);
    c[ImGuiCol_WindowBg] = bg;
    c[ImGuiCol_ChildBg] = bg;
    c[ImGuiCol_PopupBg] = bg;
    c[ImGuiCol_Border] = divider;
    c[ImGuiCol_BorderShadow] = ImVec4(0, 0, 0, 0);
    c[ImGuiCol_FrameBg] = panel;
    c[ImGuiCol_FrameBgHovered] = panelHighlight;
    c[ImGuiCol_FrameBgActive] = buttonHoverBg;
    c[ImGuiCol_TitleBg] = bg;
    c[ImGuiCol_TitleBgActive] = panel;
    c[ImGuiCol_TitleBgCollapsed] = bg;
    c[ImGuiCol_MenuBarBg] = panel;
    c[ImGuiCol_ScrollbarBg] = bg;
    c[ImGuiCol_ScrollbarGrab] = buttonBg;
    c[ImGuiCol_ScrollbarGrabHovered] = buttonHoverBg;
    c[ImGuiCol_ScrollbarGrabActive] = accent;
    c[ImGuiCol_CheckMark] = accent;
    c[ImGuiCol_SliderGrab] = accent;
    c[ImGuiCol_SliderGrabActive] = accentActive;
    c[ImGuiCol_Button] = buttonBg;
    c[ImGuiCol_ButtonHovered] = buttonHoverBg;
    c[ImGuiCol_ButtonActive] = accentActive;
    c[ImGuiCol_Header] = accentedBg;
    c[ImGuiCol_HeaderHovered] = accentHover;
    c[ImGuiCol_HeaderActive] = accentActive;
    c[ImGuiCol_Separator] = divider;
    c[ImGuiCol_SeparatorHovered] = accentHover;
    c[ImGuiCol_SeparatorActive] = accentActive;
    c[ImGuiCol_ResizeGrip] = buttonBg;
    c[ImGuiCol_ResizeGripHovered] = accentHover;
    c[ImGuiCol_ResizeGripActive] = accentActive;
    c[ImGuiCol_Tab] = panel;
    c[ImGuiCol_TabHovered] = accentHover;
    c[ImGuiCol_TabSelected] = accent;
    c[ImGuiCol_TabDimmed] = bg;
    c[ImGuiCol_TabDimmedSelected] = panel;
    c[ImGuiCol_DockingPreview] = accentedBg;
    c[ImGuiCol_DockingEmptyBg] = bg;
    c[ImGuiCol_TextSelectedBg] = accentedBg;
    c[ImGuiCol_DragDropTarget] = accentHover;
    c[ImGuiCol_NavCursor] = accent;
    c[ImGuiCol_NavWindowingHighlight] = accentHover;
    c[ImGuiCol_PlotLinesHovered] = error;
    c[ImGuiCol_PlotHistogramHovered] = error;

    // YouTubeのチップ・サムネイル・ボタンは丸みが強いため角丸を大きめにする
    style.WindowRounding = 8.0f;
    style.FrameRounding = 8.0f;
    style.GrabRounding = 8.0f;
    style.TabRounding = 8.0f;
    style.PopupRounding = 8.0f;
    style.ChildRounding = 8.0f;
    return style;
}

/// @brief YouTubeのダークテーマに近い配色を組み立てる（BuildYouTubeLightStyleのダーク版）
///        （出典: 各種配色まとめサイトで広く一致する値。#0f0f0f/#212121/#272727/#aaaaaa/#ff0000）
ImGuiStyle BuildYouTubeDarkStyle() {
    LogScope scope;
    ImGuiStyle style;
    ImGui::StyleColorsDark(&style);
    ImVec4 *c = style.Colors;

    const ImVec4 bg(0.059f, 0.059f, 0.059f, 1.00f);          // 背景: #0f0f0f
    const ImVec4 panel(0.129f, 0.129f, 0.129f, 1.00f);       // メニュー・引き出し背景: #212121
    const ImVec4 panelHighlight(0.153f, 0.153f, 0.153f, 1.00f); // チップ・ホバー: #272727
    const ImVec4 buttonBg(0.129f, 0.129f, 0.129f, 1.00f);
    const ImVec4 buttonHoverBg(0.188f, 0.188f, 0.188f, 1.00f); // 区切り線相当: #303030
    const ImVec4 accent(1.000f, 0.000f, 0.000f, 1.00f);      // ブランドカラーの赤: #ff0000
    const ImVec4 accentHover(1.000f, 0.160f, 0.160f, 1.00f);
    const ImVec4 accentActive(0.840f, 0.000f, 0.000f, 1.00f);
    const ImVec4 accentedBg(1.000f, 0.000f, 0.000f, 0.15f);  // accentをalpha<0.15>
    const ImVec4 fg(1.000f, 1.000f, 1.000f, 1.00f);          // 文字色: #ffffff
    const ImVec4 divider(0.188f, 0.188f, 0.188f, 1.00f);     // #303030
    // 公式に明確なエラー色の定義は無いため、赤いアクセントとの区別のみを目的とした近似値を使用
    const ImVec4 error(0.973f, 0.400f, 0.322f, 1.00f);

    c[ImGuiCol_Text] = fg;
    c[ImGuiCol_TextDisabled] = ImVec4(fg.x, fg.y, fg.z, 0.55f);
    c[ImGuiCol_WindowBg] = bg;
    c[ImGuiCol_ChildBg] = bg;
    c[ImGuiCol_PopupBg] = panelHighlight;
    c[ImGuiCol_Border] = divider;
    c[ImGuiCol_BorderShadow] = ImVec4(0, 0, 0, 0);
    c[ImGuiCol_FrameBg] = panel;
    c[ImGuiCol_FrameBgHovered] = panelHighlight;
    c[ImGuiCol_FrameBgActive] = buttonHoverBg;
    c[ImGuiCol_TitleBg] = bg;
    c[ImGuiCol_TitleBgActive] = panel;
    c[ImGuiCol_TitleBgCollapsed] = bg;
    c[ImGuiCol_MenuBarBg] = panel;
    c[ImGuiCol_ScrollbarBg] = bg;
    c[ImGuiCol_ScrollbarGrab] = buttonBg;
    c[ImGuiCol_ScrollbarGrabHovered] = buttonHoverBg;
    c[ImGuiCol_ScrollbarGrabActive] = accent;
    c[ImGuiCol_CheckMark] = accent;
    c[ImGuiCol_SliderGrab] = accent;
    c[ImGuiCol_SliderGrabActive] = accentActive;
    c[ImGuiCol_Button] = buttonBg;
    c[ImGuiCol_ButtonHovered] = buttonHoverBg;
    c[ImGuiCol_ButtonActive] = accentActive;
    c[ImGuiCol_Header] = accentedBg;
    c[ImGuiCol_HeaderHovered] = accentHover;
    c[ImGuiCol_HeaderActive] = accentActive;
    c[ImGuiCol_Separator] = divider;
    c[ImGuiCol_SeparatorHovered] = accentHover;
    c[ImGuiCol_SeparatorActive] = accentActive;
    c[ImGuiCol_ResizeGrip] = buttonBg;
    c[ImGuiCol_ResizeGripHovered] = accentHover;
    c[ImGuiCol_ResizeGripActive] = accentActive;
    c[ImGuiCol_Tab] = panel;
    c[ImGuiCol_TabHovered] = accentHover;
    c[ImGuiCol_TabSelected] = accent;
    c[ImGuiCol_TabDimmed] = bg;
    c[ImGuiCol_TabDimmedSelected] = panel;
    c[ImGuiCol_DockingPreview] = accentedBg;
    c[ImGuiCol_DockingEmptyBg] = bg;
    c[ImGuiCol_TextSelectedBg] = accentedBg;
    c[ImGuiCol_DragDropTarget] = accentHover;
    c[ImGuiCol_NavCursor] = accent;
    c[ImGuiCol_NavWindowingHighlight] = accentHover;
    c[ImGuiCol_PlotLinesHovered] = error;
    c[ImGuiCol_PlotHistogramHovered] = error;

    style.WindowRounding = 8.0f;
    style.FrameRounding = 8.0f;
    style.GrabRounding = 8.0f;
    style.TabRounding = 8.0f;
    style.PopupRounding = 8.0f;
    style.ChildRounding = 8.0f;
    return style;
}

/// @brief Googleのマテリアルデザイン系UIに近い配色を組み立てる。背景は白をベースに、
///        Google系アプリのアイコン・ローディングスピナー等で使われる赤・青・黄・緑の4色を
///        単なる「青がメイン」ではなく、役割ごとに複数の場所へ意図的に散りばめて使用する
///        （出典: Googleのブランドカラーとして広く知られる値。#4285F4/#EA4335/#FBBC05/#34A853）
ImGuiStyle BuildGoogleStyle() {
    LogScope scope;
    ImGuiStyle style;
    ImGui::StyleColorsLight(&style);
    ImVec4 *c = style.Colors;

    const ImVec4 bg(1.000f, 1.000f, 1.000f, 1.00f);          // 背景: #ffffff
    const ImVec4 panel(0.973f, 0.976f, 0.980f, 1.00f);       // パネル背景（Google Grey 50）: #f8f9fa
    const ImVec4 panelHighlight(0.933f, 0.937f, 0.941f, 1.00f); // panelを少し暗く
    const ImVec4 buttonBg(0.973f, 0.976f, 0.980f, 1.00f);
    const ImVec4 buttonHoverBg(0.855f, 0.863f, 0.878f, 1.00f); // 境界線（Google Grey 300）: #dadce0
    const ImVec4 fg(0.125f, 0.129f, 0.141f, 1.00f);          // 文字色（Google Grey 900）: #202124
    const ImVec4 divider(0.855f, 0.863f, 0.878f, 1.00f);     // #dadce0

    // Google特有の4色（青をメインの操作色、赤・黄・緑を他の役割に割り当てて随所に使う）
    const ImVec4 blue(0.259f, 0.522f, 0.957f, 1.00f);        // Googleブルー: #4285F4（主要な操作色）
    const ImVec4 blueHover(0.410f, 0.619f, 0.966f, 1.00f);
    const ImVec4 blueActive(0.108f, 0.424f, 0.948f, 1.00f);
    const ImVec4 blueBg(0.259f, 0.522f, 0.957f, 0.12f);      // blueをalpha<0.12>
    const ImVec4 red(0.918f, 0.263f, 0.208f, 1.00f);         // Googleレッド: #EA4335（強調・エラー・ドラッグ中）
    const ImVec4 yellow(0.984f, 0.737f, 0.020f, 1.00f);      // Googleイエロー: #FBBC05（ホバー・注意を引く箇所）
    const ImVec4 green(0.204f, 0.659f, 0.325f, 1.00f);       // Googleグリーン: #34A853（チェック・確定・ドロップ先）

    c[ImGuiCol_Text] = fg;
    c[ImGuiCol_TextDisabled] = ImVec4(fg.x, fg.y, fg.z, 0.55f);
    c[ImGuiCol_WindowBg] = bg;
    c[ImGuiCol_ChildBg] = bg;
    c[ImGuiCol_PopupBg] = bg;
    c[ImGuiCol_Border] = divider;
    c[ImGuiCol_BorderShadow] = ImVec4(0, 0, 0, 0);
    c[ImGuiCol_FrameBg] = panel;
    c[ImGuiCol_FrameBgHovered] = panelHighlight;
    c[ImGuiCol_FrameBgActive] = buttonHoverBg;
    c[ImGuiCol_TitleBg] = bg;
    c[ImGuiCol_TitleBgActive] = panel;
    c[ImGuiCol_TitleBgCollapsed] = bg;
    c[ImGuiCol_MenuBarBg] = panel;
    c[ImGuiCol_ScrollbarBg] = bg;
    c[ImGuiCol_ScrollbarGrab] = buttonBg;
    c[ImGuiCol_ScrollbarGrabHovered] = buttonHoverBg;
    c[ImGuiCol_ScrollbarGrabActive] = red;               // スクロールバーを掴んでいる間は赤
    c[ImGuiCol_CheckMark] = green;                       // チェックマークは緑（「確定」のイメージ）
    c[ImGuiCol_SliderGrab] = blue;
    c[ImGuiCol_SliderGrabActive] = yellow;               // スライダー操作中は黄色
    c[ImGuiCol_Button] = buttonBg;
    c[ImGuiCol_ButtonHovered] = buttonHoverBg;
    c[ImGuiCol_ButtonActive] = blueActive;
    c[ImGuiCol_Header] = blueBg;
    c[ImGuiCol_HeaderHovered] = blueHover;
    c[ImGuiCol_HeaderActive] = blueActive;
    c[ImGuiCol_Separator] = divider;
    c[ImGuiCol_SeparatorHovered] = blueHover;
    c[ImGuiCol_SeparatorActive] = blueActive;
    c[ImGuiCol_ResizeGrip] = buttonBg;
    c[ImGuiCol_ResizeGripHovered] = yellow;              // リサイズハンドルはホバーで黄色に
    c[ImGuiCol_ResizeGripActive] = red;                  // ドラッグ中は赤に
    c[ImGuiCol_Tab] = panel;
    c[ImGuiCol_TabHovered] = blueHover;
    c[ImGuiCol_TabSelected] = blue;
    c[ImGuiCol_TabDimmed] = bg;
    c[ImGuiCol_TabDimmedSelected] = panel;
    c[ImGuiCol_DockingPreview] = green;                  // ドッキング先のプレビューは緑（「ここに置ける」の合図）
    c[ImGuiCol_DockingEmptyBg] = bg;
    c[ImGuiCol_TextSelectedBg] = blueBg;
    c[ImGuiCol_DragDropTarget] = green;                  // ドラッグ&ドロップの受け入れ先も緑
    c[ImGuiCol_NavCursor] = blue;
    c[ImGuiCol_NavWindowingHighlight] = yellow;          // Alt+Tab相当のウィンドウ切り替えは黄色でハイライト
    c[ImGuiCol_PlotLinesHovered] = red;
    c[ImGuiCol_PlotHistogramHovered] = red;

    // Material Designは角丸が特徴的なため大きめにする
    style.WindowRounding = 8.0f;
    style.FrameRounding = 8.0f;
    style.GrabRounding = 8.0f;
    style.TabRounding = 8.0f;
    style.PopupRounding = 8.0f;
    style.ChildRounding = 8.0f;
    return style;
}

} // namespace

void EditorPreferences::RefreshFontFileList() {
    LogScope scope;
    fontFiles_ = ProjectPaths::ListAssetFiles({ ".ttf", ".otf" });
    hasScannedFontFiles_ = true;
}

void EditorPreferences::EnsureDefaultPresets() {
    LogScope scope;
    hasEnsuredDefaultPresets_ = true;

    // Dark/Light/Classicは初回（プリセットが1つも無い場合）のみまとめて登録する。
    // ユーザーが意図的に全削除していた場合、再起動のたびに復活してしまうのを防ぐため
    if (UserSettings::GetColorPresetNames().empty()) {
        ImGuiStyle temp;
        ImGui::StyleColorsDark(&temp);
        UserSettings::SetColorPreset("Dark", ColorsToJSON(temp.Colors));
        ImGui::StyleColorsLight(&temp);
        UserSettings::SetColorPreset("Light", ColorsToJSON(temp.Colors));
        ImGui::StyleColorsClassic(&temp);
        UserSettings::SetColorPreset("Classic", ColorsToJSON(temp.Colors));
    }

    // Unityプリセットは既定3種とは別に、1度だけ追加する（プリセットが1つも無くても
    // 個別に消されていても、二重追加やユーザー削除後の復活が起きないよう専用フラグで管理する）
    if (!UserSettings::GetBool("editorUI.presets.unityAdded", false)) {
        UserSettings::SetColorPreset("Unity", ColorsToJSON(BuildUnityStyle().Colors));
        UserSettings::SetBool("editorUI.presets.unityAdded", true);
    }

    // Misskeyプリセット（Mi Dark/Mi Light）も同様に、専用フラグで1度だけ追加する
    if (!UserSettings::GetBool("editorUI.presets.misskeyAdded", false)) {
        UserSettings::SetColorPreset("Misskey-Mi Dark", ColorsToJSON(BuildMisskeyMiDarkStyle().Colors));
        UserSettings::SetBool("editorUI.presets.misskeyAdded", true);
    }
    if (!UserSettings::GetBool("editorUI.presets.misskeyMiLightAdded", false)) {
        UserSettings::SetColorPreset("Misskey-Mi Light", ColorsToJSON(BuildMisskeyMiLightStyle().Colors));
        UserSettings::SetBool("editorUI.presets.misskeyMiLightAdded", true);
    }

    // Discordの既定ダークテーマ「アッシュ」プリセットも同様に、専用フラグで1度だけ追加する
    if (!UserSettings::GetBool("editorUI.presets.discordAshAdded", false)) {
        UserSettings::SetColorPreset("Discord-Ash", ColorsToJSON(BuildDiscordAshStyle().Colors));
        UserSettings::SetBool("editorUI.presets.discordAshAdded", true);
    }

    // Steamクライアント風プリセットも同様に、専用フラグで1度だけ追加する
    if (!UserSettings::GetBool("editorUI.presets.steamAdded", false)) {
        UserSettings::SetColorPreset("Steam", ColorsToJSON(BuildSteamStyle().Colors));
        UserSettings::SetBool("editorUI.presets.steamAdded", true);
    }

    // GitHub風プリセットも同様に、専用フラグで1度だけ追加する
    if (!UserSettings::GetBool("editorUI.presets.githubAdded", false)) {
        UserSettings::SetColorPreset("GitHub", ColorsToJSON(BuildGitHubStyle().Colors));
        UserSettings::SetBool("editorUI.presets.githubAdded", true);
    }

    // YouTube風プリセット（ライト/ダーク）も同様に、専用フラグで1度だけ追加する
    if (!UserSettings::GetBool("editorUI.presets.youtubeAdded", false)) {
        UserSettings::SetColorPreset("YouTube-Light", ColorsToJSON(BuildYouTubeLightStyle().Colors));
        UserSettings::SetColorPreset("YouTube-Dark", ColorsToJSON(BuildYouTubeDarkStyle().Colors));
        UserSettings::SetBool("editorUI.presets.youtubeAdded", true);
    }

    // Google風プリセットも同様に、専用フラグで1度だけ追加する
    if (!UserSettings::GetBool("editorUI.presets.googleAdded", false)) {
        UserSettings::SetColorPreset("Google", ColorsToJSON(BuildGoogleStyle().Colors));
        UserSettings::SetBool("editorUI.presets.googleAdded", true);
    }
}

void EditorPreferences::ShowStyleSection() {
    LogScope scope;
    if (!ImGui::TreeNode(TranslationLabel("editor.preferences.style"))) return;
    ImGui::TextDisabled("%s", TranslationC("editor.preferences.style.description"));

    // ScaleAllSizes適用前（uiScale=1.0相当）の素の既定値。UserSettingsに未保存の項目のフォールバックに使う
    const ImGuiStyle defaultStyle{};
    JSON styleJson = UserSettings::GetJSON("editorUI.style", JSON::object());
    if (!styleJson.is_object()) styleJson = JSON::object();

    bool changed = false;
    auto slider = [&](const char *labelKey, const char *jsonKey, float defaultValue, float minValue, float maxValue) {
        auto it = styleJson.find(jsonKey);
        float value = (it != styleJson.end() && it->is_number()) ? it->get<float>() : defaultValue;
        if (ImGui::SliderFloat(TranslationLabel(labelKey), &value, minValue, maxValue, "%.1f")) {
            styleJson[jsonKey] = value;
            changed = true;
        }
    };

    ImGui::SeparatorText(TranslationLabel("editor.preferences.style.rounding"));
    slider("editor.preferences.style.windowrounding", "windowRounding", defaultStyle.WindowRounding, 0.0f, 16.0f);
    slider("editor.preferences.style.childrounding", "childRounding", defaultStyle.ChildRounding, 0.0f, 16.0f);
    slider("editor.preferences.style.framerounding", "frameRounding", defaultStyle.FrameRounding, 0.0f, 16.0f);
    slider("editor.preferences.style.popuprounding", "popupRounding", defaultStyle.PopupRounding, 0.0f, 16.0f);
    slider("editor.preferences.style.scrollbarrounding", "scrollbarRounding", defaultStyle.ScrollbarRounding, 0.0f, 16.0f);
    slider("editor.preferences.style.grabrounding", "grabRounding", defaultStyle.GrabRounding, 0.0f, 16.0f);
    slider("editor.preferences.style.tabrounding", "tabRounding", defaultStyle.TabRounding, 0.0f, 16.0f);

    ImGui::SeparatorText(TranslationLabel("editor.preferences.style.bordersize"));
    slider("editor.preferences.style.windowbordersize", "windowBorderSize", defaultStyle.WindowBorderSize, 0.0f, 4.0f);
    slider("editor.preferences.style.childbordersize", "childBorderSize", defaultStyle.ChildBorderSize, 0.0f, 4.0f);
    slider("editor.preferences.style.framebordersize", "frameBorderSize", defaultStyle.FrameBorderSize, 0.0f, 4.0f);
    slider("editor.preferences.style.popupbordersize", "popupBorderSize", defaultStyle.PopupBorderSize, 0.0f, 4.0f);
    slider("editor.preferences.style.tabbarbordersize", "tabBarBorderSize", defaultStyle.TabBarBorderSize, 0.0f, 4.0f);

    ImGui::SeparatorText(TranslationLabel("editor.preferences.style.spacing"));
    slider("editor.preferences.style.windowpaddingx", "windowPaddingX", defaultStyle.WindowPadding.x, 0.0f, 40.0f);
    slider("editor.preferences.style.windowpaddingy", "windowPaddingY", defaultStyle.WindowPadding.y, 0.0f, 40.0f);
    slider("editor.preferences.style.framepaddingx", "framePaddingX", defaultStyle.FramePadding.x, 0.0f, 40.0f);
    slider("editor.preferences.style.framepaddingy", "framePaddingY", defaultStyle.FramePadding.y, 0.0f, 40.0f);
    slider("editor.preferences.style.itemspacingx", "itemSpacingX", defaultStyle.ItemSpacing.x, 0.0f, 40.0f);
    slider("editor.preferences.style.itemspacingy", "itemSpacingY", defaultStyle.ItemSpacing.y, 0.0f, 40.0f);
    slider("editor.preferences.style.iteminnerspacingx", "itemInnerSpacingX", defaultStyle.ItemInnerSpacing.x, 0.0f, 40.0f);
    slider("editor.preferences.style.iteminnerspacingy", "itemInnerSpacingY", defaultStyle.ItemInnerSpacing.y, 0.0f, 40.0f);
    slider("editor.preferences.style.indentspacing", "indentSpacing", defaultStyle.IndentSpacing, 0.0f, 40.0f);
    slider("editor.preferences.style.scrollbarsize", "scrollbarSize", defaultStyle.ScrollbarSize, 1.0f, 40.0f);
    slider("editor.preferences.style.grabminsize", "grabMinSize", defaultStyle.GrabMinSize, 1.0f, 40.0f);

    if (changed) {
        UserSettings::SetJSON("editorUI.style", styleJson);
    }

    ImGui::Spacing();
    if (ImGui::Button(TranslationLabel("editor.preferences.style.reset"))) {
        UserSettings::SetJSON("editorUI.style", JSON::object());
    }

    ImGui::TreePop();
}

void EditorPreferences::ShowKeyBindingRow(const char *labelKey, const std::string &action, ImGuiKeyChord defaultChord) {
    LogScope scope;
    ImGui::PushID(action.c_str());

    ImGui::TextUnformatted(TranslationC(labelKey));
    ImGui::SameLine(200.0f);

    const bool isListening = (listeningKeyBindingAction_ == action);
    const ImGuiKeyChord current = EditorKeyBindings::Get(action, defaultChord);
    const std::string buttonLabel = isListening
        ? Translation("editor.preferences.keybindings.pressany")
        : EditorKeyBindings::ToDisplayString(current);
    if (ImGui::Button(buttonLabel.c_str(), ImVec2(160.0f, 0.0f))) {
        listeningKeyBindingAction_ = isListening ? std::string() : action;
    }

    if (isListening) {
        if (ImGui::IsKeyPressed(ImGuiKey_Escape, false)) {
            listeningKeyBindingAction_.clear();
        } else {
            const ImGuiKeyChord captured = EditorKeyBindings::CaptureChordThisFrame();
            if (captured != ImGuiKey_None) {
                EditorKeyBindings::Set(action, captured);
                listeningKeyBindingAction_.clear();
            }
        }
    }

    ImGui::SameLine();
    if (ImGui::SmallButton(TranslationLabel("editor.common.reset"))) {
        EditorKeyBindings::Set(action, defaultChord);
        if (isListening) listeningKeyBindingAction_.clear();
    }

    ImGui::PopID();
}

void EditorPreferences::ShowKeyBindingsSection() {
    LogScope scope;
    if (!ImGui::TreeNode(TranslationLabel("editor.preferences.keybindings"))) return;
    ImGui::TextDisabled("%s", TranslationC("editor.preferences.keybindings.description"));

    ShowKeyBindingRow("editor.menu.edit.undo", "Undo", ImGuiMod_Ctrl | ImGuiKey_Z);
    ShowKeyBindingRow("editor.menu.edit.redo", "Redo", ImGuiMod_Ctrl | ImGuiKey_Y);
    ShowKeyBindingRow("editor.menu.file.savescene", "SaveScene", ImGuiMod_Ctrl | ImGuiKey_S);

    ImGui::Spacing();
    if (ImGui::Button(TranslationLabel("editor.preferences.keybindings.resetall"))) {
        EditorKeyBindings::ResetAll();
        listeningKeyBindingAction_.clear();
    }

    ImGui::TreePop();
}

void EditorPreferences::ShowLayoutSection() {
    LogScope scope;
    if (!ImGui::TreeNode(TranslationLabel("editor.preferences.layout"))) return;
    ImGui::TextDisabled("%s", TranslationC("editor.preferences.layout.description"));

    const std::vector<std::string> layoutPresetNames = UserSettings::GetLayoutPresetNames();
    if (ImGui::BeginTable("##LayoutPresets", 2, ImGuiTableFlags_SizingFixedFit)) {
        for (const std::string &name : layoutPresetNames) {
            ImGui::PushID(name.c_str());
            ImGui::TableNextRow();
            ImGui::TableNextColumn();
            if (ImGui::Button(name.c_str())) {
                // 次フレーム開始時（NewFrame()より前）に適用されるよう予約する。
                // ここ（フレーム途中、多数のウィンドウが既にBeginされた後）で直接
                // LoadIniSettingsFromMemoryを呼ぶと、この呼び出しより後に描画される
                // ウィンドウ（環境設定より後に描画される翻訳キー設定ウィンドウや、
                // SceneEditor::ShowImGui()全体より後に描画されるビューア系ウィンドウ等）
                // だけドッキングが解除されてしまう
                ImGuiManager::RequestLoadIniSettings(UserSettings::GetLayoutPreset(name));
            }
            ImGui::TableNextColumn();
            if (ImGui::SmallButton(TranslationLabel("editor.common.delete"))) {
                UserSettings::DeleteLayoutPreset(name);
            }
            ImGui::PopID();
        }
        ImGui::EndTable();
    }

    ImGui::SetNextItemWidth(200.0f);
    ImGui::InputTextWithHint("##NewLayoutPresetName", TranslationC("editor.preferences.layout.name.hint"), &newLayoutPresetNameBuffer_);
    ImGui::SameLine();
    ImGui::BeginDisabled(newLayoutPresetNameBuffer_.empty());
    if (ImGui::Button(TranslationLabel("editor.preferences.layout.save"))) {
        size_t iniSize = 0;
        const char *iniData = ImGui::SaveIniSettingsToMemory(&iniSize);
        UserSettings::SetLayoutPreset(newLayoutPresetNameBuffer_, std::string(iniData, iniSize));
        newLayoutPresetNameBuffer_.clear();
    }
    ImGui::EndDisabled();
    ImGui::SetItemTooltip("%s", TranslationC("editor.preferences.layout.save.tooltip"));

    ImGui::Spacing();
    if (ImGui::Button(TranslationLabel("editor.preferences.layout.resetdefault"))) {
        ImGuiManager::ResetDockLayoutToDefault();
    }
    ImGui::SetItemTooltip("%s", TranslationC("editor.preferences.layout.resetdefault.tooltip"));

    ImGui::TreePop();
}

void EditorPreferences::ShowLanguageSection() {
    LogScope scope;
    ImGui::SeparatorText(TranslationLabel("editor.preferences.language"));

    const std::vector<std::string> languages = GetLoadedLanguages();
    const std::string &currentLanguage = GetCurrentLanguage();

    ImGui::SetNextItemWidth(200.0f);
    if (ImGui::BeginCombo(TranslationLabel("editor.preferences.language.select"), GetLanguageDisplayName(currentLanguage).c_str())) {
        for (const auto &lang : languages) {
            const bool selected = (lang == currentLanguage);
            ImGui::PushID(lang.c_str());
            if (ImGui::Selectable(GetLanguageDisplayName(lang).c_str(), selected) && !selected) {
                SetCurrentLanguage(lang);
                UserSettings::SetString("editorUI.language", lang);
            }
            if (selected) ImGui::SetItemDefaultFocus();
            ImGui::PopID();
        }
        ImGui::EndCombo();
    }
    ImGui::SetItemTooltip("%s", TranslationC("editor.preferences.language.tooltip"));

    // アプリケーション（ゲーム）側の表示言語は、エディター自身の表示言語（上のコンボ）とは独立している。
    // ここはPlayモードでの動作確認用に、エディターから手動で切り替えるための入り口
    const std::string &currentApplicationLanguage = GetCurrentApplicationLanguage();

    ImGui::SetNextItemWidth(200.0f);
    if (ImGui::BeginCombo(TranslationLabel("editor.preferences.language.application_select"), GetLanguageDisplayName(currentApplicationLanguage).c_str())) {
        for (const auto &lang : languages) {
            const bool selected = (lang == currentApplicationLanguage);
            ImGui::PushID(lang.c_str());
            if (ImGui::Selectable(GetLanguageDisplayName(lang).c_str(), selected) && !selected) {
                SetCurrentApplicationLanguage(lang);
                PlayerSettings::SetString(PlayerSettings::kLanguageKey, lang);
            }
            if (selected) ImGui::SetItemDefaultFocus();
            ImGui::PopID();
        }
        ImGui::EndCombo();
    }
    ImGui::SetItemTooltip("%s", TranslationC("editor.preferences.language.application_select.tooltip"));
}

void EditorPreferences::ShowImGui() {
    LogScope scope;
    if (!ImGui::Begin(TranslationLabel("editor.preferences.window"))) {
        ImGui::End();
        return;
    }
    DrawFloatingWindowChromeButtons();

    ImGui::TextDisabled("%s", TranslationC("editor.preferences.description"));

    //--------- 表示言語 ---------//
    ShowLanguageSection();

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
    const std::vector<std::string> presetNames = UserSettings::GetColorPresetNames();

    ImGui::TextDisabled("%s", TranslationC("editor.preferences.presets.description"));
    if (ImGui::BeginTable("##ColorPresets", 2, ImGuiTableFlags_SizingFixedFit)) {
        for (const std::string &name : presetNames) {
            const JSON colors = UserSettings::GetColorPreset(name, JSON());
            const bool isValidPreset = colors.is_array() && colors.size() == ImGuiCol_COUNT;
            ImGui::PushID(name.c_str());
            ImGui::TableNextRow();
            ImGui::TableNextColumn();
            ImGui::BeginDisabled(!isValidPreset);
            if (ImGui::Button(name.c_str())) {
                UserSettings::SetJSON("editorUI.colors", colors);
            }
            ImGui::EndDisabled();
            ImGui::TableNextColumn();
            if (ImGui::SmallButton(TranslationLabel("editor.common.delete"))) {
                UserSettings::DeleteColorPreset(name);
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
        UserSettings::SetColorPreset(newPresetNameBuffer_, ColorsToJSON(ImGui::GetStyle().Colors));
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

    //--------- 詳細スタイル（角丸・境界線太さ・余白等） ---------//
    ShowStyleSection();

    //--------- キーバインド ---------//
    ShowKeyBindingsSection();

    //--------- ドッキングレイアウト ---------//
    ShowLayoutSection();

    ImGui::Spacing();
    if (ImGui::Button(TranslationLabel("editor.preferences.resetall"))) {
        UserSettings::SetFloat("editorUI.fontScale", 1.0f);
        UserSettings::SetFloat("editorUI.uiScale", 1.0f);
        UserSettings::SetString("editorUI.fontPath", "");
        ImGuiStyle temp;
        ImGui::StyleColorsDark(&temp);
        UserSettings::SetJSON("editorUI.colors", ColorsToJSON(temp.Colors));
        UserSettings::SetJSON("editorUI.style", JSON::object());
    }

    ImGui::End();
}

} // namespace KashipanEngine
#endif // USE_IMGUI
