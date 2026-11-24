#pragma once
#include "Utilities/Conversion/ConvertString.h"
#include <algorithm>
#include <cctype>
#include <filesystem>
#include <string>

namespace KashipanEngine::AssetPath {

/// @brief 文字列内のパス区切り文字（'/' または '\\'）をすべて '/' に変換する
/// @param s パス区切り文字を含む文字列
/// @return 正規化された文字列
inline std::string NormalizePathSlashes(std::string s) {
    std::replace(s.begin(), s.end(), '\\', '/');
    while (!s.empty() && s.back() == '/') s.pop_back();
    return s;
}


/// @brief 文字列を小文字に変換する（ASCII文字のみ対応）
/// @param s 変換する文字列
/// @return 小文字に変換された文字列
inline std::string ToLower(std::string s) {
    for (auto &c : s) c = static_cast<char>(std::tolower(static_cast<unsigned char>(c)));
    return s;
}

/// @brief 指定されたパスをアセットルートからの相対パスに変換する
/// @param assetsRoot アセットルートのパス
/// @param fullPath フルパス
/// @return アセットルートからの相対パス。計算に失敗した場合はファイル名のみを返す
inline std::string MakeAssetRelativePath(const std::string &assetsRoot, const std::string &fullPath) {
    const std::filesystem::path root = Utf8StringToPath(assetsRoot);
    const std::filesystem::path full = Utf8StringToPath(fullPath);

    std::error_code ec;
    auto rel = std::filesystem::relative(full, root, ec);
    if (ec) {
        return NormalizePathSlashes(PathToUtf8String(full.filename()));
    }
    return NormalizePathSlashes(PathToUtf8String(rel));
}

} // namespace KashipanEngine::AssetPath
