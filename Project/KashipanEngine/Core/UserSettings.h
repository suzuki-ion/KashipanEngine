#pragma once
#include <string>

#include "Utilities/FileIO/JSON.h"

namespace KashipanEngine {

/// @brief プロジェクトをまたいで共有されるユーザー個人の設定ストア
/// @details エンジンルート直下の UserSettings.json に保存される。
///          エディターUIの配色・フォント・表示倍率のように「どのプロジェクトを開いていても
///          同じであってほしい」設定と、前回開いたプロジェクト名を扱う。
///          プロジェクトごとに異なるエディターの状態（ツリーの開閉など）は EditorSettings 側。
///          値の変更時に即座にJSONファイルへ保存される。
class UserSettings final {
public:
    /// @brief 前回開いたプロジェクト名を保持するキー
    static constexpr const char *kLastOpenedProjectKey = "project.lastOpened";

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

private:
    /// @brief 保存先ファイル名（エンジンルート直下）
    static constexpr const char *kFileName = "UserSettings.json";

    /// @brief 保存先の物理パスを取得する（ProjectPaths::Initialize後にのみ正しい値を返す）
    static std::string GetFilePath();

    static void EnsureLoaded();
    static void Save();

    static inline JSON sData_;
    static inline bool sLoaded_ = false;
};

} // namespace KashipanEngine
