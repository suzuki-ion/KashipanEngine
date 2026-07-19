#pragma once
#ifdef USE_IMGUI

class asIScriptEngine;

namespace KashipanEngine {

/// @brief ImGuiの主要な関数をエディターツールスクリプト用にAngelScriptへ登録する
/// @details スクリプト側からは ImGui::Text(...) / ImGui::Button(...) のように呼び出せる。
///          EditorToolManagerのスクリプトエンジンにのみ登録される（ゲームプレイ用スクリプトへは
///          登録されないため、通常のScriptComponentからは使用できない）。
///          EditorToolのUpdate()は[EditorWindow]のウィンドウが開いている間そのウィンドウの
///          Begin/End内で呼ばれるため、Update()内でこれらの関数を呼ぶとウィンドウへUIが描画される
/// @param engine 登録先のスクリプトエンジン（Vector2/Vector3/Vector4、string、arrayの登録後に呼ぶこと）
void RegisterImGuiScriptBindings(asIScriptEngine *engine);

} // namespace KashipanEngine
#endif // USE_IMGUI
