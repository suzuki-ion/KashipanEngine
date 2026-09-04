#ifdef USE_IMGUI

#include "EditorWindowChrome.h"

#include <Windows.h>

#include <imgui.h>
#include <imgui_internal.h>

#include <unordered_map>

#include "Debug/Logger.h"

namespace KashipanEngine {

namespace {

struct RestoreRect {
    ImVec2 pos;
    ImVec2 size;
};

// 最大化前の位置・サイズをウィンドウIDごとに記憶しておく
std::unordered_map<ImGuiID, RestoreRect> &GetRestoreRects() {
    LogScope scope;
    static std::unordered_map<ImGuiID, RestoreRect> rects;
    return rects;
}

// タイトルバー上のボタン1個を描画し、クリックされたらtrueを返す。
// ImGui標準のアイテム機構(ItemAdd/ButtonBehavior)は使わず、フォアグラウンド描画と
// マウス座標の直接判定だけで完結させている。Begin()が既にタイトルバー全域を
// ウィンドウ移動のクリック判定に使った後でこの関数が呼ばれるため、標準機構を
// 経由するとクリップ矩形やアクティブID管理と噛み合わず正しく反応しないため
bool DrawChromeButton(ImDrawList *drawList, const ImRect &bb, bool isHovered) {
    LogScope scope;
    const ImU32 bgCol = isHovered
        ? ImGui::GetColorU32(ImGuiCol_ButtonHovered)
        : IM_COL32(0, 0, 0, 0);
    drawList->AddRectFilled(bb.Min, bb.Max, bgCol, ImGui::GetStyle().FrameRounding);
    return isHovered && ImGui::IsMouseClicked(ImGuiMouseButton_Left);
}

void DrawMinimizeGlyph(ImDrawList *drawList, const ImRect &bb, ImU32 color) {
    LogScope scope;
    const float inset = bb.GetWidth() * 0.28f;
    const float y = bb.Max.y - bb.GetHeight() * 0.32f;
    drawList->AddLine(ImVec2(bb.Min.x + inset, y), ImVec2(bb.Max.x - inset, y), color, 1.5f);
}

void DrawMaximizeGlyph(ImDrawList *drawList, const ImRect &bb, ImU32 color, bool isMaximized) {
    LogScope scope;
    const float inset = bb.GetWidth() * 0.26f;
    if (!isMaximized) {
        const ImRect r(bb.Min.x + inset, bb.Min.y + inset, bb.Max.x - inset, bb.Max.y - inset);
        drawList->AddRect(r.Min, r.Max, color, 0.0f, 0, 1.5f);
        return;
    }
    // 元に戻す(Restore)アイコン: 奥にもう一枚重なった矩形で表現する
    const float offset = bb.GetWidth() * 0.12f;
    const ImRect back(bb.Min.x + inset + offset, bb.Min.y + inset, bb.Max.x - inset, bb.Max.y - inset - offset);
    const ImRect front(bb.Min.x + inset, bb.Min.y + inset + offset, bb.Max.x - inset - offset, bb.Max.y - inset);
    drawList->AddRect(back.Min, back.Max, color, 0.0f, 0, 1.5f);
    drawList->AddRectFilled(front.Min, front.Max, ImGui::GetColorU32(ImGuiCol_WindowBg));
    drawList->AddRect(front.Min, front.Max, color, 0.0f, 0, 1.5f);
}

} // namespace

void DrawFloatingWindowChromeButtons() {
    LogScope scope;
    ImGuiContext &g = *GImGui;
    ImGuiWindow *window = g.CurrentWindow;
    if (!window || window->Collapsed) return;
    if (!(g.IO.ConfigFlags & ImGuiConfigFlags_ViewportsEnable)) return;
    if (ImGui::IsWindowDocked()) return;

    ImGuiViewport *viewport = window->Viewport;
    if (!viewport || viewport == ImGui::GetMainViewport()) return;

    HWND hwnd = static_cast<HWND>(viewport->PlatformHandleRaw);
    if (!hwnd) return;

    const ImRect titleBarRect = window->TitleBarRect();
    const float pad = g.Style.FramePadding.x;
    const float btnSize = g.FontSize;
    const float centerY = titleBarRect.Min.y + (titleBarRect.GetHeight() - btnSize) * 0.5f;

    float rightEdge = titleBarRect.Max.x - pad;
    if (window->HasCloseButton) {
        // 標準のクローズボタン分のスペースを空けておく
        rightEdge -= (btnSize + pad);
    }

    const ImRect maximizeBb(ImVec2(rightEdge - btnSize, centerY), ImVec2(rightEdge, centerY + btnSize));
    const ImRect minimizeBb(ImVec2(maximizeBb.Min.x - pad - btnSize, centerY), ImVec2(maximizeBb.Min.x - pad, centerY + btnSize));

    const bool minimizeHovered = minimizeBb.Contains(g.IO.MousePos);
    const bool maximizeHovered = maximizeBb.Contains(g.IO.MousePos);

    // タイトルバー領域はBegin()内で既にウィンドウ移動のクリック判定に使われているため、
    // 自前ボタンの上でのクリックによる意図しないウィンドウ移動開始を打ち消しておく
    if ((minimizeHovered || maximizeHovered) && g.IO.MouseClicked[0] && g.MovingWindow == window) {
        g.MovingWindow = nullptr;
    }

    ImDrawList *drawList = ImGui::GetForegroundDrawList(viewport);
    const ImU32 iconCol = ImGui::GetColorU32(ImGuiCol_Text);

    if (DrawChromeButton(drawList, minimizeBb, minimizeHovered)) {
        ShowWindow(hwnd, SW_MINIMIZE);
    }
    DrawMinimizeGlyph(drawList, minimizeBb, iconCol);

    auto &restoreRects = GetRestoreRects();
    const ImGuiID windowId = window->ID;
    const auto restoreIt = restoreRects.find(windowId);
    const bool isMaximized = restoreIt != restoreRects.end();

    if (DrawChromeButton(drawList, maximizeBb, maximizeHovered)) {
        if (isMaximized) {
            ImGui::SetWindowPos(restoreIt->second.pos, ImGuiCond_Always);
            ImGui::SetWindowSize(restoreIt->second.size, ImGuiCond_Always);
            restoreRects.erase(restoreIt);
        } else if (const ImGuiPlatformMonitor *monitor = ImGui::GetViewportPlatformMonitor(viewport)) {
            restoreRects[windowId] = RestoreRect{window->Pos, window->Size};
            ImGui::SetWindowPos(monitor->WorkPos, ImGuiCond_Always);
            ImGui::SetWindowSize(monitor->WorkSize, ImGuiCond_Always);
        }
    }
    DrawMaximizeGlyph(drawList, maximizeBb, iconCol, isMaximized);
}

} // namespace KashipanEngine

#endif // USE_IMGUI
