#pragma once
#include <string>
#include <vector>

#include "Utilities/FileIO/JSON.h"

namespace KashipanEngine {

/// @brief プロジェクトをまたいで共有されるユーザー個人の設定ストア
/// @details エンジンルート直下の UserSettings/ フォルダに保存される。
///          エディターUIの配色・フォント・表示倍率のように「どのプロジェクトを開いていても
///          同じであってほしい」設定と、前回開いたプロジェクト名を扱う。
///          プロジェクトごとに異なるエディターの状態（ツリーの開閉など）は EditorSettings 側。
///          値の変更時に即座にファイルへ保存される。
///          配色プリセット・レイアウトプリセットのように多数の名前付き項目を持つものは
///          外部からの追加・削除がしやすいよう UserSettings/ColorPresets/<名前>.json、
///          UserSettings/LayoutPresets/<名前>.ini のように1項目1ファイルで保存する。
///          それ以外の単一の値は UserSettings/Settings.json にまとめて保存する。
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

    /// @brief 配色プリセット名の一覧を取得する（UserSettings/ColorPresets/ 配下の .json ファイル名）
    static std::vector<std::string> GetColorPresetNames();

    /// @brief 配色プリセットを取得する（要素数ImGuiCol_COUNTのRGBA配列。存在しない/形式不正ならdefaultValue）
    static JSON GetColorPreset(const std::string &name, const JSON &defaultValue);

    /// @brief 配色プリセットを保存する（UserSettings/ColorPresets/<name>.json）
    static void SetColorPreset(const std::string &name, const JSON &colors);

    /// @brief 配色プリセットを削除する
    static void DeleteColorPreset(const std::string &name);

    /// @brief レイアウトプリセット名の一覧を取得する（UserSettings/LayoutPresets/ 配下の .ini ファイル名）
    static std::vector<std::string> GetLayoutPresetNames();

    /// @brief レイアウトプリセットを取得する（ImGuiの生iniテキスト。存在しない場合は空文字）
    static std::string GetLayoutPreset(const std::string &name);

    /// @brief レイアウトプリセットを保存する（UserSettings/LayoutPresets/<name>.ini）
    static void SetLayoutPreset(const std::string &name, const std::string &iniText);

    /// @brief レイアウトプリセットを削除する
    static void DeleteLayoutPreset(const std::string &name);

private:
    /// @brief 保存先フォルダ名（エンジンルート直下）
    static constexpr const char *kFolderName = "UserSettings";
    /// @brief 単一の値をまとめて保存するファイル名（kFolderName配下）
    static constexpr const char *kSettingsFileName = "Settings.json";
    /// @brief 移行前の旧形式（単一ファイル）のファイル名（エンジンルート直下）
    static constexpr const char *kLegacyFileName = "UserSettings.json";
    /// @brief 配色プリセットを1項目1ファイルで保存するサブフォルダ名
    static constexpr const char *kColorPresetsSubfolder = "ColorPresets";
    /// @brief レイアウトプリセットを1項目1ファイルで保存するサブフォルダ名
    static constexpr const char *kLayoutPresetsSubfolder = "LayoutPresets";

    /// @brief 単一の値をまとめたファイルの物理パスを取得する（ProjectPaths::Initialize後にのみ正しい値を返す）
    static std::string GetSettingsFilePath();
    /// @brief 配色プリセットフォルダの物理パスを取得する
    static std::string GetColorPresetsFolderPath();
    /// @brief レイアウトプリセットフォルダの物理パスを取得する
    static std::string GetLayoutPresetsFolderPath();

    static void EnsureLoaded();
    static void Save();

    /// @brief 旧形式（単一ファイルのUserSettings.json）が残っている場合、新しいフォルダ構成へ1度だけ移行する。
    ///        移行元ファイルは削除せず UserSettings.json.bak にリネームして残す
    static void MigrateFromLegacyFileIfNeeded();

    static inline JSON sData_;
    static inline bool sLoaded_ = false;
};

} // namespace KashipanEngine
