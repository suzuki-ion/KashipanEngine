#pragma once
#include "Assets/AssetPath.h"
#include "Assets/CaseInsensitive.h"
#include <algorithm>
#include <string>
#include <unordered_map>
#include <utility>
#include <vector>

namespace KashipanEngine::AssetRegistry {

/// @brief アセットのエントリを登録し、ファイル名とアセットパスの索引を更新する
/// @tparam Handle アセットのハンドル型
/// @tparam Entry アセットのエントリ型
/// @param handle アセットのハンドル
/// @param entries アセットのエントリを保持するマップ
/// @param fileNames アセットのファイル名をキーとしてハンドルを検索するマップ
/// @param assetPaths アセットのパスをキーとしてハンドルを検索するマップ
/// @param entry アセットのエントリ
/// @return 登録されたアセットのハンドル、または登録に失敗した場合は空のハンドル
template<typename Handle, typename Entry>
Handle Register(Handle handle, std::unordered_map<Handle, Entry> &entries,
    FileMap<Handle> &fileNames, FileMap<Handle> &assetPaths, Entry &&entry) {
    if (handle == Handle{} || entries.contains(handle)) return Handle{};

    fileNames[entry.fileName] = handle;
    assetPaths[AssetPath::NormalizePathSlashes(entry.assetPath)] = handle;
    entries.emplace(handle, std::move(entry));
    return handle;
}

/// @brief 指定されたキーに対応するハンドルを検索する
/// @tparam Handle アセットのハンドル型
/// @param index アセットのハンドルを検索するためのマップ
/// @param key 検索するキー
/// @return 指定されたキーに対応するハンドル、または見つからなかった場合は空のハンドル
template<typename Handle>
Handle FindHandle(const FileMap<Handle> &index, const std::string &key) {
    auto it = index.find(key);
    return it == index.end() ? Handle{} : it->second;
}

/// @brief ソートされたアセットパスのリストを取得する
/// @tparam Handle アセットのハンドル型
/// @tparam Entry アセットのエントリ型
/// @param entries アセットのエントリを保持するマップ
/// @return ソートされたアセットパスのリスト
template<typename Handle, typename Entry>
std::vector<std::string> GetSortedAssetPaths(const std::unordered_map<Handle, Entry> &entries) {
    std::vector<std::string> paths;
    paths.reserve(entries.size());
    for (const auto &entry : entries) paths.push_back(entry.second.assetPath);
    std::sort(paths.begin(), paths.end());
    return paths;
}

/// @brief 登録済みアセットのファイル名・フルパス・相対パスと検索索引を変更する
/// @tparam Handle アセットのハンドル型
/// @tparam Entry アセットのエントリ型
/// @param entries アセットのエントリを保持するマップ
/// @param fileNames アセットのファイル名をキーとしてハンドルを検索するマップ
/// @param assetPaths アセットのパスをキーとしてハンドルを検索するマップ
/// @param oldAssetPath 変更前のアセットのパス
/// @param newAssetPath 変更後のアセットのパス
/// @param assetsRoot アセットのルートパス
/// @return アセットの名前やパスの変更が成功した場合は true、失敗した場合は false
template<typename Handle, typename Entry>
bool RenameFileAsset(std::unordered_map<Handle, Entry> &entries,
    FileMap<Handle> &fileNames, FileMap<Handle> &assetPaths,
    const std::string &oldAssetPath, const std::string &newAssetPath, const std::string &assetsRoot) {
    auto pathIt = assetPaths.find(AssetPath::NormalizePathSlashes(oldAssetPath));
    if (pathIt == assetPaths.end()) return false;
    const Handle handle = pathIt->second;
    auto entryIt = entries.find(handle);
    if (entryIt == entries.end()) return false;

    auto &entry = entryIt->second;
    assetPaths.erase(pathIt);
    fileNames.erase(entry.fileName);
    entry.assetPath = AssetPath::NormalizePathSlashes(newAssetPath);
    entry.fileName = PathToUtf8String(Utf8StringToPath(entry.assetPath).filename());
    entry.fullPath = assetsRoot + "/" + entry.assetPath;

    assetPaths[entry.assetPath] = handle;
    fileNames[entry.fileName] = handle;
    return true;
}

/// @brief アセットのパスを変更する
/// @tparam Handle アセットのハンドル型
/// @tparam Entry アセットのエントリ型
/// @param entries アセットのエントリを保持するマップ
/// @param fileNames アセットのファイル名をキーとしてハンドルを検索するマップ
/// @param assetPaths アセットのパスをキーとしてハンドルを検索するマップ
/// @param oldAssetPath 変更前のアセットのパス
/// @param newAssetPath 変更後のアセットのパス
/// @return アセットのパスの変更が成功した場合は true、失敗した場合は false
template<typename Handle, typename Entry>
bool RenameMediaAsset(std::unordered_map<Handle, Entry> &entries,
    FileMap<Handle> &fileNames, FileMap<Handle> &assetPaths,
    const std::string &oldAssetPath, const std::string &newAssetPath) {
    auto pathIt = assetPaths.find(AssetPath::NormalizePathSlashes(oldAssetPath));
    if (pathIt == assetPaths.end()) return false;

    const Handle handle = pathIt->second;
    assetPaths.erase(pathIt);

    const std::string normalizedNew = AssetPath::NormalizePathSlashes(newAssetPath);
    assetPaths[normalizedNew] = handle;

    auto entryIt = entries.find(handle);
    if (entryIt != entries.end()) {
        auto &entry = entryIt->second;
        entry.assetPath = normalizedNew;
        const std::string newFileName = PathToUtf8String(Utf8StringToPath(newAssetPath).filename());
        fileNames.erase(entry.fileName);
        entry.fileName = newFileName;
        fileNames[newFileName] = handle;
    }
    return true;
}

} // namespace KashipanEngine::AssetRegistry
