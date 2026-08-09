#pragma once
#ifdef USE_IMGUI

#include <imgui.h>
#include <string>
#include <vector>
#include "Utilities/Passkeys.h"

namespace KashipanEngine {

class SceneEditor;

/// @brief エディターUI（表示倍率・フォント・配色・詳細スタイル・キーバインド・
///        ドッキングレイアウト・表示言語）の個人設定パネル
/// @details 値はすべてUserSettings経由で保存され、プロジェクトをまたいで共有される。
///          ImGuiManagerがUserSettingsを直接読みに行って適用するため、
///          このクラスからImGuiManagerへ値を渡すことはしない
class EditorPreferences final {
public:
    EditorPreferences(Passkey<SceneEditor>) {}

    void ShowImGui();

private:
    /// @brief Assets配下のフォントファイル一覧を再スキャンする（毎フレーム呼ばないこと。
    ///        再帰的なディレクトリ走査はコストが高く、フレーム毎に行うとFPSが大きく低下する）
    void RefreshFontFileList();
    /// @brief 配色プリセット（UserSettingsの"editorUI.presets"）が未登録の場合、
    ///        Dark/Light/Classicの既定プリセットを1度だけ登録する
    void EnsureDefaultPresets();
    /// @brief 角丸・境界線太さ・余白等、色以外のImGuiStyle項目の詳細設定UIを表示する
    void ShowStyleSection();
    /// @brief キーバインド（ショートカットキー）の再割り当てUIを表示する
    void ShowKeyBindingsSection();
    /// @brief 1つのアクションに対するキーバインド行（現在の割り当て・再割り当てボタン・個別リセット）を表示する
    void ShowKeyBindingRow(const char *labelKey, const std::string &action, ImGuiKeyChord defaultChord);
    /// @brief ドッキングレイアウトのプリセット保存・切り替え・既定配置への復元UIを表示する
    void ShowLayoutSection();
    /// @brief 表示言語の切り替えUIを表示する
    void ShowLanguageSection();

    std::vector<std::string> fontFiles_;
    bool hasScannedFontFiles_ = false;
    bool hasEnsuredDefaultPresets_ = false;
    std::string newPresetNameBuffer_;
    std::string newLayoutPresetNameBuffer_;
    /// @brief 現在キー入力待ち（再割り当て中）のアクション名。空文字なら待機中のものは無い
    std::string listeningKeyBindingAction_;
};

} // namespace KashipanEngine

#endif // USE_IMGUI
