#pragma once
#include "Utilities/FileIO/ContentCache.h"
#include <DirectXTex.h>
#include <algorithm>
#include <cctype>
#include <utility>
namespace KashipanEngine::PreparedTextureCache {
inline bool Eligible(const std::filesystem::path &source) {
    auto ext = source.extension().string();
    std::transform(ext.begin(), ext.end(), ext.begin(), [](unsigned char c) { return static_cast<char>(std::tolower(c)); });
    return ext == ".png" || ext == ".jpg" || ext == ".jpeg" || ext == ".bmp" || ext == ".tga" || ext == ".tif" || ext == ".tiff" || ext == ".webp";
}
inline std::string Key(const std::filesystem::path &source) {
    return "Prepared2D-v1-RGBA8-TEX_FILTER_SRGB-fullmips|" + ContentCache::PathText(std::filesystem::absolute(source).lexically_normal());
}
inline std::filesystem::path CachePath(const std::filesystem::path &directory, const std::filesystem::path &source) {
    return directory / (ContentCache::Digest(Key(source)) + ".cache");
}
inline bool Load(const std::filesystem::path &source, const std::filesystem::path &directory, DirectX::ScratchImage &image) {
    if (!Eligible(source)) return false;
    const auto bytes = ContentCache::Load(CachePath(directory, source), Key(source));
    if (!bytes) return false;
    DirectX::TexMetadata metadata{};
    const HRESULT hr = DirectX::LoadFromDDSMemory(reinterpret_cast<const uint8_t *>(bytes->data()), bytes->size(), DirectX::DDS_FLAGS_NONE, &metadata, image);
    return SUCCEEDED(hr) && metadata.dimension == DirectX::TEX_DIMENSION_TEXTURE2D && metadata.arraySize == 1 && !metadata.IsCubemap();
}
inline bool Store(const std::filesystem::path &source, const std::filesystem::path &directory,
    const DirectX::ScratchImage &image, const ContentCache::Json &sourceDependency) {
    if (!Eligible(source) || !image.GetImageCount()) return false;
    const auto &metadata = image.GetMetadata();
    if (metadata.dimension != DirectX::TEX_DIMENSION_TEXTURE2D || metadata.arraySize != 1 || metadata.IsCubemap()) return false;
    DirectX::Blob dds;
    if (FAILED(DirectX::SaveToDDSMemory(image.GetImages(), image.GetImageCount(), metadata, DirectX::DDS_FLAGS_NONE, dds))) return false;
    return ContentCache::Store(CachePath(directory, source), Key(source), ContentCache::Json::array({sourceDependency}), dds.GetBufferPointer(), dds.GetBufferSize());
}
inline DirectX::ScratchImage ConvertAndGenerateMips(DirectX::ScratchImage scratch, const DirectX::TexMetadata& meta, DXGI_FORMAT dstFormat) {
    DirectX::ScratchImage converted;
    if (meta.format != dstFormat) {
        const HRESULT hr = DirectX::Convert(scratch.GetImages(), scratch.GetImageCount(), scratch.GetMetadata(), dstFormat, DirectX::TEX_FILTER_SRGB, DirectX::TEX_THRESHOLD_DEFAULT, converted);
        if (FAILED(hr)) return DirectX::ScratchImage();
    }
    DirectX::ScratchImage finalImage = (meta.format == dstFormat) ? std::move(scratch) : std::move(converted);

    // ミップマップ生成
    DirectX::ScratchImage mipChain;
    if (DirectX::IsCompressed(finalImage.GetMetadata().format)) {
        // 圧縮形式の場合はミップマップ生成をスキップ
        mipChain = std::move(finalImage);
    } else {
        HRESULT hr = DirectX::GenerateMipMaps(finalImage.GetImages(), finalImage.GetImageCount(), finalImage.GetMetadata(), DirectX::TEX_FILTER_SRGB, 0, mipChain);
        if (FAILED(hr)) {
            // ミップマップ生成に失敗した場合は元画像をそのまま使う
            const DirectX::Image *baseImg = finalImage.GetImages();
            if (!baseImg || !baseImg->pixels) {
                return DirectX::ScratchImage();
            }
            hr = mipChain.InitializeFromImage(*baseImg);
            if (FAILED(hr)) {
                return DirectX::ScratchImage();
            }
        }
    }

    const DirectX::Image* img0 = mipChain.GetImages();
    if (!img0 || !img0->pixels) {
        return DirectX::ScratchImage();
    }

    return mipChain;
}

inline DirectX::ScratchImage PrepareFile(const std::filesystem::path &source) {
    DirectX::ScratchImage decoded;
    DirectX::TexMetadata metadata{};
    auto ext = source.extension().string();
    std::transform(ext.begin(), ext.end(), ext.begin(), [](unsigned char c) { return static_cast<char>(std::tolower(c)); });
    const HRESULT hr = ext == ".tga"
        ? DirectX::LoadFromTGAFile(source.c_str(), &metadata, decoded)
        : DirectX::LoadFromWICFile(source.c_str(), DirectX::WIC_FLAGS_FORCE_RGB, &metadata, decoded);
    if (FAILED(hr)) return {};
    return ConvertAndGenerateMips(std::move(decoded), metadata, DXGI_FORMAT_R8G8B8A8_UNORM);
}
} // namespace KashipanEngine::PreparedTextureCache
