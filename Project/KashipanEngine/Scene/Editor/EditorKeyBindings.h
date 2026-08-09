#pragma once
#ifdef USE_IMGUI
#include <imgui.h>
#include <string>

namespace KashipanEngine {

/// @brief エディターのショートカットキー（キーバインド）の個人設定
/// @details 値はUserSettings経由で保存され、プロジェクトをまたいで共有される。
///          アクション名（"Undo"等）をキーに、割り当てられたImGuiKeyChordを保持する。
///          未割り当て（保存されていない）のアクションは呼び出し元が渡した既定値を使う。
class EditorKeyBindings final {
public:
    /// @brief アクションに割り当てられたキーチョードを取得する
    /// @param action アクション名（例: "Undo"）
    /// @param defaultChord 未設定の場合に返す既定のキーチョード
    static ImGuiKeyChord Get(const std::string &action, ImGuiKeyChord defaultChord);
    /// @brief アクションへキーチョードを割り当てる
    static void Set(const std::string &action, ImGuiKeyChord chord);
    /// @brief 全アクションの割り当てを解除する（次回取得時は既定値に戻る）
    static void ResetAll();

    /// @brief キーチョードを表示用文字列へ変換する（例: "Ctrl+Shift+Z"）
    static std::string ToDisplayString(ImGuiKeyChord chord);

    /// @brief このフレームで押された修飾キー以外のキーを、現在の修飾キー状態と合わせて
    ///        キーチョードとして取得する（再割り当てUIの入力キャプチャ用）
    /// @return 何も押されていない場合は ImGuiKey_None
    static ImGuiKeyChord CaptureChordThisFrame();

private:
    /// @brief UserSettingsへの保存キー（アクション名ごとのサブキー）
    static constexpr const char *kSettingsKey = "editorUI.keybindings";
};

} // namespace KashipanEngine

#endif // USE_IMGUI
