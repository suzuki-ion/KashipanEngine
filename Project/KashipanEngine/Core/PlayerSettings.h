#pragma once
#include <string>

#include "Utilities/FileIO/JSON.h"

namespace KashipanEngine {

/// @brief プレイヤー（配布したゲームの実行環境）ごとの設定を保存するストア
/// @details プロジェクトルート直下（Assetsの外）に保存される。EditorSettingsと同じ理由で
///          Assetsの外に置く：Assetsをテンプレートとして配布・コピーした際に、
///          プレイヤーの状態が紛れ込まないようにするため。
///          UserSettingsとの違いは、UserSettingsが「全プロジェクト共有のエディター個人設定」で
///          USE_IMGUI専用に近い使われ方をするのに対し、こちらはReleaseビルドでも動作し、
///          表示言語のようにゲームをプレイする側の選択を保存する用途を想定している。
///          値の変更時に即座にファイルへ保存される。
class PlayerSettings final {
public:
    /// @brief 表示言語を保持するキー
    static constexpr const char *kLanguageKey = "language";

    /// @brief bool値の取得
    static bool GetBool(const std::string &key, bool defaultValue);

    /// @brief bool値の設定（変更があった場合のみ保存する）
    static void SetBool(const std::string &key, bool value);

    /// @brief int値の取得
    static int GetInt(const std::string &key, int defaultValue);

    /// @brief int値の設定（変更があった場合のみ保存する）
    static void SetInt(const std::string &key, int value);

    /// @brief float値の取得
    static float GetFloat(const std::string &key, float defaultValue);

    /// @brief float値の設定（変更があった場合のみ保存する）
    static void SetFloat(const std::string &key, float value);

    /// @brief string値の取得
    static std::string GetString(const std::string &key, const std::string &defaultValue);

    /// @brief string値の設定（変更があった場合のみ保存する）
    static void SetString(const std::string &key, const std::string &value);

    /// @brief Vector2値の取得
    static Vector2 GetVector2(const std::string &key, const Vector2 &defaultValue);

    /// @brief Vector2値の設定（変更があった場合のみ保存する）
    static void SetVector2(const std::string &key, const Vector2 &value);

    /// @brief Vector3値の取得
    static Vector3 GetVector3(const std::string &key, const Vector3 &defaultValue);

    /// @brief Vector3値の設定（変更があった場合のみ保存する）
    static void SetVector3(const std::string &key, const Vector3 &value);

    /// @brief Vector4値の取得
    static Vector4 GetVector4(const std::string &key, const Vector4 &defaultValue);

    /// @brief Vector4値の設定（変更があった場合のみ保存する）
    static void SetVector4(const std::string &key, const Vector4 &value);

    /// @brief Quaternion値の取得
    static Quaternion GetQuaternion(const std::string &key, const Quaternion &defaultValue);

    /// @brief Quaternion値の設定（変更があった場合のみ保存する）
    static void SetQuaternion(const std::string &key, const Quaternion &value);

    /// @brief JSON値の取得（上記の型で用意しきれていない任意の形状の値を保存したい場合に使う。
    ///        配列・オブジェクトなど、型を限定せず保存できる）
    static JSON GetJSON(const std::string &key, const JSON &defaultValue);

    /// @brief JSON値の設定（変更があった場合のみ保存する）
    static void SetJSON(const std::string &key, const JSON &value);

private:
    /// @brief 保存先ファイル名（プロジェクトルート直下）
    static constexpr const char *kFileName = "PlayerSettings.json";

    /// @brief 保存先の物理パスを取得する
    static std::string GetFilePath();

    static void EnsureLoaded();
    static void Save();

    static inline JSON sData_;
    static inline bool sLoaded_ = false;
};

} // namespace KashipanEngine
