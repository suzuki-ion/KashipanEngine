#pragma once
#include <string>

namespace KashipanEngine {

/// @brief 翻訳設定ファイル(.json)を読み込む
bool LoadTranslationFile(const std::string &filePath);

/// @brief キーに対応する翻訳テキストを取得する
/// @param lang 言語コード
/// @param key 翻訳キー
/// @return 翻訳テキスト
const std::string &GetTranslationText(const std::string &lang, const std::string &key);

/// @brief キーに対応する翻訳テキストを取得する（現在の言語設定を使用）
/// @param key 翻訳キー
/// @return 翻訳テキスト
const std::string &GetTranslationText(const std::string &key);

/// @brief 現在の言語コードを取得する
/// @return 言語コード
const std::string &GetCurrentLanguage();

} // namespace KashipanEngine