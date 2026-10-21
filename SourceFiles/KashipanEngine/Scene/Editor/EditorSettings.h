#pragma once
#ifdef USE_IMGUI
#include <string>
#include "Utilities/FileIO/JSON.h"

namespace KashipanEngine {

/// @brief エディターUIの状態（開閉状況など）をexe再起動をまたいで保存する設定ストア
/// @details 値の変更時に即座にJSONファイルへ保存される。
///          保存先はプロジェクトルート直下で、プロジェクトごとに独立している。
///          Assets外に置くのは、配布用にAssetsをコピーした際にエディターの状態が混入しないようにするため。
///          配色やフォントなどプロジェクトをまたいで共有したい設定は UserSettings 側で扱う。
class EditorSettings final {
public:
    /// @brief bool値の取得
    static bool GetBool(const std::string &key, bool defaultValue);

    /// @brief bool値の設定（変更があった場合のみ保存する）
    static void SetBool(const std::string &key, bool value);

    /// @brief float値の取得
    static float GetFloat(const std::string &key, float defaultValue);

    /// @brief float値の設定（変更があった場合のみ保存する）
    static void SetFloat(const std::string &key, float value);

    /// @brief string値の取得
    static std::string GetString(const std::string &key, const std::string &defaultValue);

    /// @brief string値の設定（変更があった場合のみ保存する）
    static void SetString(const std::string &key, const std::string &value);

    /// @brief JSON値の取得（配列・オブジェクトなど任意の形状の値を保存したい場合に使う）
    static JSON GetJSON(const std::string &key, const JSON &defaultValue);

    /// @brief JSON値の設定（変更があった場合のみ保存する）
    static void SetJSON(const std::string &key, const JSON &value);

    /// @brief 開閉状態を保存するツリーノード（デフォルトは開いた状態）
    /// @details 開閉状態は key で保存され、再起動後も維持される。
    static bool PersistentTreeNode(const char *label, const std::string &key, bool defaultOpen = true);

    /// @brief 開閉状態を保存する折りたたみヘッダー（デフォルトは開いた状態）
    static bool PersistentCollapsingHeader(const char *label, const std::string &key, bool defaultOpen = true);

private:
    /// @brief 保存先ファイル名（プロジェクトルート直下）
    static constexpr const char *kFileName = "EditorSettings.json";

    /// @brief 保存先の物理パスを取得する
    static std::string GetFilePath();

    static void EnsureLoaded();
    static void Save();

    static inline JSON sData_;
    static inline bool sLoaded_ = false;
};

} // namespace KashipanEngine
#endif // USE_IMGUI
