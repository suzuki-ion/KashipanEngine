#pragma once
#ifdef USE_IMGUI

#include <string>
#include <vector>
#include "Utilities/Passkeys.h"

namespace KashipanEngine {

class SceneEditor;

/// @brief エディターUI（表示倍率・フォント・配色）の個人設定パネル
/// @details 値はすべてEditorSettings経由で保存される。ImGuiManagerがEditorSettingsを
///          直接読みに行って適用するため、このクラスからImGuiManagerへ値を渡すことはしない
class EditorPreferences final {
public:
    EditorPreferences(Passkey<SceneEditor>) {}

    void ShowImGui();

private:
    /// @brief Assets配下のフォントファイル一覧を再スキャンする（毎フレーム呼ばないこと。
    ///        再帰的なディレクトリ走査はコストが高く、フレーム毎に行うとFPSが大きく低下する）
    void RefreshFontFileList();

    std::vector<std::string> fontFiles_;
    bool hasScannedFontFiles_ = false;
};

} // namespace KashipanEngine

#endif // USE_IMGUI
