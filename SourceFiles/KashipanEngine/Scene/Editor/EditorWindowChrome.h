#pragma once
#ifdef USE_IMGUI

namespace KashipanEngine {

/// @brief ImGui::Begin() の直後に呼び出す。
/// @details ドッキングを外れて独立したOSウィンドウ（セカンダリビューポート）として
///          浮いているウィンドウのタイトルバー右上に、最小化・最大化/元に戻すボタンを描画する。
///          ドッキング中のウィンドウやメインビューポート上のウィンドウでは何もしない。
///          最小化は対象ウィンドウのHWNDへ ShowWindow(SW_MINIMIZE) を発行することで、
///          タスクバーへ実際に収納される本来の最小化として動作する。
void DrawFloatingWindowChromeButtons();

} // namespace KashipanEngine

#endif // USE_IMGUI
