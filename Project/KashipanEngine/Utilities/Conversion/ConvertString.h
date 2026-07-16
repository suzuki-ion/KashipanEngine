#pragma once
#include <cstdint>
#include <filesystem>
#include <string>
#include <vector>

namespace KashipanEngine {

/// @brief 文字列変換
/// @param str 変換元文字列
/// @return 変換後文字列
std::wstring ConvertString(const std::string &str);

/// @brief 文字列変換
/// @param str 変換元文字列
/// @return 変換後文字列
std::string ConvertString(const std::wstring &str);

/// @brief ShiftJISからUTF-16へ変換
/// @param sjis 変換元ShiftJIS文字列
/// @return 変換後UTF-16文字列
std::wstring ShiftJISToUTF16(const std::string &sjis);

/// @brief UTF-16からShiftJISへ変換
/// @param utf16 変換元UTF-16文字列
/// @return 変換後ShiftJIS文字列
std::string UTF16ToUTF8(const std::wstring &utf16);

/// @brief ShiftJISからUTF-8へ変換
/// @param sjis 変換元ShiftJIS文字列
/// @return 変換後UTF-8文字列
std::string ShiftJISToUTF8(const std::string &sjis);

/// @brief UTF-8文字列をUnicodeコードポイント（char32_t）列へ分解する
/// @details 不正なバイト列は1バイトずつU+FFFD（置換文字）として扱う
/// @param utf8 変換元UTF-8文字列
/// @return コードポイントの列
std::vector<char32_t> Utf8ToCodepoints(const std::string &utf8);

/// @brief std::filesystem::pathをUTF-8のstd::stringへ変換する
/// @details std::filesystem::path::string()はWindows上で現在のANSIコードページ（Shift-JIS等）を
///          使って変換するため、そのコードページで表現できない文字（多言語のファイル名等）を含む
///          パスは文字化けし、以後どこで復元しようとしても手遅れになる。
///          エンジン内では「narrow文字列は常にUTF-8」という前提で扱うため、
///          std::filesystem::pathからnarrow文字列を得る際は必ずこちらを使うこと（.string()は使わない）。
std::string PathToUtf8String(const std::filesystem::path &path);

/// @brief UTF-8のstd::stringからstd::filesystem::pathを構築する
/// @details std::filesystem::path(const std::string&)はWindows上で現在のANSIコードページとして
///          解釈するため、UTF-8前提の文字列（本エンジンの規約）を渡すと文字化けする。
///          UTF-8文字列からpathを構築する際は必ずこちらを使うこと（pathコンストラクタへ直接渡さない）。
std::filesystem::path Utf8StringToPath(const std::string &utf8String);

} // namespace KashipanEngine

