#pragma once
#ifdef USE_IMGUI
#include <string>
#include "Utilities/FileIO/JSON.h"

namespace KashipanEngine {

/// @brief エディターUIの状態（開閉状況など）をexe再起動をまたいで保存する設定ストア
/// @details 値の変更時に即座にJSONファイルへ保存される。
class EditorSettings final {
public:
    /// @brief bool値の取得
    static bool GetBool(const std::string &key, bool defaultValue);

    /// @brief bool値の設定（変更があった場合のみ保存する）
    static void SetBool(const std::string &key, bool value);

    /// @brief 開閉状態を保存するツリーノード（デフォルトは開いた状態）
    /// @details 開閉状態は key で保存され、再起動後も維持される。
    static bool PersistentTreeNode(const char *label, const std::string &key, bool defaultOpen = true);

    /// @brief 開閉状態を保存する折りたたみヘッダー（デフォルトは開いた状態）
    static bool PersistentCollapsingHeader(const char *label, const std::string &key, bool defaultOpen = true);

private:
    static constexpr const char *kFilePath = "Assets/KashipanEngine/EditorSettings.json";

    static void EnsureLoaded();

    static inline JSON sData_;
    static inline bool sLoaded_ = false;
};

} // namespace KashipanEngine
#endif // USE_IMGUI
