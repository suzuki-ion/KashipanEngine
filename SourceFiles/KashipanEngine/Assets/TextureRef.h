#pragma once
#include <string>

/// @brief マテリアルのextraParametersでテクスチャ型パラメータを扱うためのラッパー型。
/// @details ハンドルは保持せず、Assetsルートからの相対パスのみ保持する。実際の
///          TextureManager::TextureHandleへの解決は使用側（描画時のバインド処理等）が
///          都度TextureManager::GetTextureFromAssetPathで行う
struct TextureRef final {
    TextureRef() noexcept = default;
    explicit TextureRef(std::string path) noexcept : assetPath(std::move(path)) {}

    bool operator==(const TextureRef &other) const noexcept { return assetPath == other.assetPath; }
    bool operator!=(const TextureRef &other) const noexcept { return !(*this == other); }

    std::string assetPath;
};
