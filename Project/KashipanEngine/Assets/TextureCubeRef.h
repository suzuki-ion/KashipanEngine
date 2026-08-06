#pragma once
#include <string>

/// @brief マテリアルのextraParametersでキューブマップ型パラメータ（環境マップ等）を扱うためのラッパー型。
/// @details TextureRefと同じくハンドルは保持せずAssetsルートからの相対パスのみ保持する。TextureRefと
///          別型にしているのは、編集UIのピッカーで`TextureManager::TextureListEntry::isCubemap`により
///          候補を絞り込み、平面画像を誤ってTextureCube型のシェーダースロットへ割り当てられないようにするため
///          （2D用のSRVをTextureCubeスロットへバインドするとGPU側で未定義動作になりうる）
struct TextureCubeRef final {
    TextureCubeRef() noexcept = default;
    explicit TextureCubeRef(std::string path) noexcept : assetPath(std::move(path)) {}

    bool operator==(const TextureCubeRef &other) const noexcept { return assetPath == other.assetPath; }
    bool operator!=(const TextureCubeRef &other) const noexcept { return !(*this == other); }

    std::string assetPath;
};
