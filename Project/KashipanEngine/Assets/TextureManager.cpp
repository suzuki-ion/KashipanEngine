#include "TextureManager.h"
#include "Assets/CaseInsensitive.h"
#include "Core/ProjectPaths.h"

#include "Core/DirectXCommon.h"
#include "Debug/Logger.h"
#include "Graphics/Resources/ShaderResourceResource.h"
#include "Utilities/Conversion/ConvertString.h"
#include "Utilities/FileIO/Directory.h"
#include "Graphics/Pipeline/System/ShaderVariableBinder.h"

#if defined(USE_IMGUI)
#include <imgui.h>
#include <imgui_internal.h>
#endif



#include <d3d12.h>
#include <d3dx12.h>
#include <wrl.h>

#include <algorithm>
#include <filesystem>
#include <functional>
#include <unordered_map>
#include <vector>

#include "Utilities/Plugin/Plugins.h"

namespace KashipanEngine {

namespace {

using Handle = TextureManager::TextureHandle;

struct TextureEntry final {
    std::string fullPath;
    std::string assetPath;
    std::string fileName;

    std::unique_ptr<ShaderResourceResource> texture;
    Microsoft::WRL::ComPtr<ID3D12Resource> upload;

    std::vector<std::unique_ptr<ShaderResourceResource>> frameViews;
    std::vector<UINT64> frameSrvGpuPtrs;
    std::vector<UINT> frameSrvIndices;

    UINT width = 0;
    UINT height = 0;
    DXGI_FORMAT format = DXGI_FORMAT_UNKNOWN;
    UINT64 srvGpuPtr = 0;
    UINT srvIndex = 0;
    UINT mipLevels = 1;
    UINT frameCount = 1;

    /// @brief 外部管理テクスチャ（ScreenBuffer等）。非nullの場合はSRV/サイズをここから毎回取得する
    const IShaderTexture *external = nullptr;
};

std::unordered_map<Handle, TextureEntry> sTextures;
FileMap<Handle> sFileNameToHandle;
FileMap<Handle> sAssetPathToHandle;

// 外部管理テクスチャ用ハンドル（SRVインデックス由来のハンドルと衝突しない上位領域を使う）
constexpr Handle kExternalHandleBase = 0x80000000u;
Handle sNextExternalHandle = kExternalHandleBase;

ID3D12Device* sDevice = nullptr;
SRVHeap* sSrvHeap = nullptr;

std::string NormalizePathSlashes(std::string s) {
    std::replace(s.begin(), s.end(), '\\', '/');
    while (!s.empty() && s.back() == '/') s.pop_back();
    return s;
}

std::string ToLower(std::string s) {
    for (auto& c : s) c = static_cast<char>(std::tolower(static_cast<unsigned char>(c)));
    return s;
}

bool HasSupportedImageExtension(const std::filesystem::path& p) {
    const std::string ext = ToLower(p.extension().string());
    return (ext == ".png" || ext == ".jpg" || ext == ".jpeg" || ext == ".bmp" || ext == ".tga" || ext == ".dds" || ext == ".hdr" || ext == ".tif" || ext == ".tiff" || ext == ".gif" || ext == ".webp");
}

std::string MakeAssetRelativePath(const std::string& assetsRoot, const std::string& fullPath) {
    const std::filesystem::path root = Utf8StringToPath(assetsRoot);
    const std::filesystem::path full = Utf8StringToPath(fullPath);

    std::error_code ec;
    auto rel = std::filesystem::relative(full, root, ec);
    if (ec) {
        return NormalizePathSlashes(PathToUtf8String(full.filename()));
    }
    return NormalizePathSlashes(PathToUtf8String(rel));
}

/// @brief 現在アクティブなTextureManagerインスタンス（ModelManager等からのテクスチャ登録用）
TextureManager* sActiveInstance = nullptr;

/// @brief デコード直後の画像を目的フォーマットへ変換し、ミップチェインを生成する
/// @details ディスク読み込み（LoadTextureFromFile）・メモリ読み込み（LoadTextureFromMemory）の
///          両方から共有される後処理
DirectX::ScratchImage ConvertAndGenerateMips(DirectX::ScratchImage scratch, const DirectX::TexMetadata& meta, DXGI_FORMAT dstFormat) {
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

Handle RegisterEntry(TextureEntry&& entry) {
    // TextureHandle は 0 を無効値とするため、SRV index に +1 した値をハンドルとして返す
    const Handle handle = static_cast<Handle>(entry.srvIndex + 1u);

    if (handle == TextureManager::kInvalidHandle) return TextureManager::kInvalidHandle;
    if (sTextures.find(handle) != sTextures.end()) return TextureManager::kInvalidHandle;

    sFileNameToHandle[entry.fileName] = handle;
    sAssetPathToHandle[NormalizePathSlashes(entry.assetPath)] = handle;
    sTextures.emplace(handle, std::move(entry));
    return handle;
}

UINT Align256(UINT v) { return (v + 255u) & ~255u; }

} // namespace

D3D12_GPU_DESCRIPTOR_HANDLE TextureManager::TextureView::GetSrvHandle() const noexcept {
    D3D12_GPU_DESCRIPTOR_HANDLE h{};
    if (handle_ == kInvalidHandle) return h;
    auto it = sTextures.find(handle_);
    if (it == sTextures.end()) return h;
    if (it->second.external) return it->second.external->GetSrvHandle();
    h.ptr = it->second.srvGpuPtr;
    return h;
}

std::uint32_t TextureManager::TextureView::GetWidth() const noexcept {
    if (handle_ == kInvalidHandle) return 0;
    auto it = sTextures.find(handle_);
    if (it == sTextures.end()) return 0;
    if (it->second.external) return it->second.external->GetWidth();
    return static_cast<std::uint32_t>(it->second.width);
}

std::uint32_t TextureManager::TextureView::GetHeight() const noexcept {
    if (handle_ == kInvalidHandle) return 0;
    auto it = sTextures.find(handle_);
    if (it == sTextures.end()) return 0;
    if (it->second.external) return it->second.external->GetHeight();
    return static_cast<std::uint32_t>(it->second.height);
}

bool TextureManager::BindTexture(ShaderVariableBinder* shaderBinder, const std::string& nameKey, const IShaderTexture& texture) {
    if (!shaderBinder) return false;

    const auto h = texture.GetSrvHandle();
    if (h.ptr == 0) return false;

    return shaderBinder->Bind(nameKey, h);
}

bool TextureManager::BindTexture(ShaderVariableBinder* shaderBinder, const std::string& nameKey, TextureHandle handle) {
    if (!shaderBinder) return false;
    if (handle == kInvalidHandle) return false;

    auto it = sTextures.find(handle);
    if (it == sTextures.end()) return false;

    D3D12_GPU_DESCRIPTOR_HANDLE h{};
    if (it->second.external) {
        h = it->second.external->GetSrvHandle();
    } else {
        h.ptr = it->second.srvGpuPtr;
    }
    if (h.ptr == 0) return false;

    return shaderBinder->Bind(nameKey, h);
}

TextureManager::TextureHandle TextureManager::RegisterExternalTexture(const std::string& name, const IShaderTexture* texture) {
    LogScope scope;
    if (!texture || name.empty()) return kInvalidHandle;
    // 同名の外部テクスチャは登録できない
    if (sFileNameToHandle.find(name) != sFileNameToHandle.end()) return kInvalidHandle;

    TextureEntry entry{};
    entry.fileName = name;
    entry.assetPath = name;
    entry.external = texture;

    const Handle handle = sNextExternalHandle++;
    sFileNameToHandle[name] = handle;
    sAssetPathToHandle[name] = handle;
    sTextures.emplace(handle, std::move(entry));
    return handle;
}

bool TextureManager::UnregisterExternalTexture(TextureHandle handle) {
    LogScope scope;
    auto it = sTextures.find(handle);
    if (it == sTextures.end() || !it->second.external) return false;
    sFileNameToHandle.erase(it->second.fileName);
    sAssetPathToHandle.erase(it->second.assetPath);
    sTextures.erase(it);
    return true;
}

bool TextureManager::UnregisterExternalTexture(const IShaderTexture* texture) {
    if (!texture) return false;
    for (const auto &kv : sTextures) {
        if (kv.second.external == texture) {
            return UnregisterExternalTexture(kv.first);
        }
    }
    return false;
}

TextureManager::TextureManager(Passkey<GameEngine>, DirectXCommon* directXCommon, const std::string& assetsRootPath)
    : directXCommon_(directXCommon), assetsRootPath_(NormalizePathSlashes(assetsRootPath)) {
    LogScope scope;
    if (directXCommon_) {
        sDevice = directXCommon_->GetDeviceForTextureManager(Passkey<TextureManager>{});
        sSrvHeap = directXCommon_->GetSRVHeapForTextureManager(Passkey<TextureManager>{});
    }
    sActiveInstance = this;
    LoadAllFromAssetsFolder();
}

TextureManager::~TextureManager() {
    LogScope scope;
    if (sActiveInstance == this) sActiveInstance = nullptr;
    sTextures.clear();
    sFileNameToHandle.clear();
    sAssetPathToHandle.clear();
    sSrvHeap = nullptr;
    sDevice = nullptr;
}

TextureManager* TextureManager::GetActiveInstance(Passkey<ModelManager>) {
    return sActiveInstance;
}

void TextureManager::LoadAllFromAssetsFolder() {
    LogScope scope;
    const auto dir = GetDirectoryData(assetsRootPath_, true, true);

    std::vector<std::string> files;
    const auto filtered = GetDirectoryDataByExtension(dir,
        { ".png", ".jpg", ".jpeg", ".bmp", ".tga", ".dds", ".hdr", ".tif", ".tiff", ".gif", ".webp" });

    std::function<void(const DirectoryData&)> flatten = [&](const DirectoryData& d) {
        for (const auto& f : d.files) files.push_back(f);
        for (const auto& sd : d.subdirectories) flatten(sd);
    };
    flatten(filtered);

    // ファイルI/O・デコード・ミップマップ生成はCPU処理のみでGPUリソースに触れないため、
    // スレッドプールで並列実行する（mipMapContainer_はshared_mutexで保護済み）
    Plugin::RunParallelAndWait(files.size(), [this, &files](size_t i) {
        mipMapContainer_.AddMipMap(files[i], LoadTextureFromFile(files[i]));
        });

    // 全ファイルのデコードが完了したら、メインスレッドでGPUリソース作成・アップロード・登録を順に行う
    for (const auto& f : files) {
        LoadTexture(f);
    }
}

TextureManager::TextureHandle TextureManager::LoadTexture(const std::string& filePath) {
	// ミップマップの生成
	const DirectX::ScratchImage* mipChain = mipMapContainer_.GetMipMap(filePath);
    if(mipChain == nullptr) {
        Log(Translation("engine.texture.loading.failed.loadfile") + filePath, LogSeverity::Error);
        return kInvalidHandle;
	}

    if(mipChain->GetImageCount() == 0) {
        Log(Translation("engine.texture.loading.failed.loadfile") + filePath, LogSeverity::Error);
        return kInvalidHandle;
	}
    const std::filesystem::path p = Utf8StringToPath(filePath);

	// メタデータからテクスチャ情報を取得
    const auto &mmeta = mipChain->GetMetadata();
    UINT mipLevels = static_cast<UINT>(mmeta.mipLevels);
	// 最初のミップレベルの情報を使用してテクスチャサイズを取得
    const DirectX::Image* img0 = mipChain->GetImages();

    TextureEntry entry{};
    entry.fullPath = NormalizePathSlashes(PathToUtf8String(p));
    entry.assetPath = MakeAssetRelativePath(assetsRootPath_, entry.fullPath);
    entry.fileName = PathToUtf8String(p.filename());
    entry.width = static_cast<UINT>(img0->width);
    entry.height = static_cast<UINT>(img0->height);
    entry.format = mmeta.format;
    entry.mipLevels = static_cast<UINT>(mmeta.mipLevels);

    const bool isCube = mmeta.IsCubemap();
    UINT arraySize = 1;
    if (isCube) {
        arraySize = 6;
    } else if (mmeta.arraySize > 1) {
        arraySize = static_cast<UINT>(mmeta.arraySize);
    }
    entry.frameCount = arraySize;

    // GPU側テクスチャ + SRV を Resources 経由で作成（COPY_DEST から開始してこの後のコピーに備える）
    if (isCube) {
        D3D12_SHADER_RESOURCE_VIEW_DESC srvDesc{};
        srvDesc.Format = entry.format;
        srvDesc.ViewDimension = D3D12_SRV_DIMENSION_TEXTURECUBE;
        srvDesc.Shader4ComponentMapping = D3D12_DEFAULT_SHADER_4_COMPONENT_MAPPING;
        srvDesc.TextureCube.MipLevels = UINT_MAX;
        srvDesc.TextureCube.MostDetailedMip = 0;
        srvDesc.TextureCube.ResourceMinLODClamp = 0.0f;
        entry.texture = std::make_unique<ShaderResourceResource>(
            entry.width,
            entry.height,
            entry.format,
            D3D12_RESOURCE_FLAG_NONE,
            nullptr,
            D3D12_RESOURCE_STATE_COPY_DEST,
            mipLevels,
            arraySize,
            &srvDesc);

    } else if (arraySize > 1) {
        D3D12_SHADER_RESOURCE_VIEW_DESC srvDesc{};
        srvDesc.Format = entry.format;
        srvDesc.ViewDimension = D3D12_SRV_DIMENSION_TEXTURE2DARRAY;
        srvDesc.Shader4ComponentMapping = D3D12_DEFAULT_SHADER_4_COMPONENT_MAPPING;
        srvDesc.Texture2DArray.MipLevels = entry.mipLevels;
        srvDesc.Texture2DArray.MostDetailedMip = 0;
        srvDesc.Texture2DArray.FirstArraySlice = 0;
        srvDesc.Texture2DArray.ArraySize = arraySize;
        srvDesc.Texture2DArray.ResourceMinLODClamp = 0.0f;
        entry.texture = std::make_unique<ShaderResourceResource>(
            entry.width,
            entry.height,
            entry.format,
            D3D12_RESOURCE_FLAG_NONE,
            nullptr,
            D3D12_RESOURCE_STATE_COPY_DEST,
            mipLevels,
            arraySize,
            &srvDesc);
    } else {
        entry.texture = std::make_unique<ShaderResourceResource>(
            entry.width,
            entry.height,
            entry.format,
            D3D12_RESOURCE_FLAG_NONE,
            nullptr,
            D3D12_RESOURCE_STATE_COPY_DEST,
            mipLevels,
            arraySize);
    } 

    {
        auto *desc = entry.texture->GetDescriptorHandleInfoForTextureManager(Passkey<TextureManager>{});
        if (!desc) {
            Log(Translation("engine.texture.loading.failed.createresource") + PathToUtf8String(p), LogSeverity::Error);
            return kInvalidHandle;
        }
        entry.srvGpuPtr = desc->gpuHandle.ptr;
        entry.srvIndex = desc->index;
        entry.frameViews.clear();
        entry.frameSrvGpuPtrs.clear();
        entry.frameSrvIndices.clear();
        entry.frameViews.reserve(arraySize);
        entry.frameSrvGpuPtrs.reserve(arraySize);
        entry.frameSrvIndices.reserve(arraySize);
    }

    if (arraySize > 1) {
        for (UINT slice = 0; slice < arraySize; ++slice) {
            D3D12_SHADER_RESOURCE_VIEW_DESC sliceDesc{};
            sliceDesc.Format = entry.format;
            sliceDesc.ViewDimension = D3D12_SRV_DIMENSION_TEXTURE2DARRAY;
            sliceDesc.Shader4ComponentMapping = D3D12_DEFAULT_SHADER_4_COMPONENT_MAPPING;
            sliceDesc.Texture2DArray.MipLevels = entry.mipLevels;
            sliceDesc.Texture2DArray.MostDetailedMip = 0;
            sliceDesc.Texture2DArray.FirstArraySlice = slice;
            sliceDesc.Texture2DArray.ArraySize = 1;
            sliceDesc.Texture2DArray.ResourceMinLODClamp = 0.0f;

            auto sliceView = std::make_unique<ShaderResourceResource>(
                entry.width,
                entry.height,
                entry.format,
                D3D12_RESOURCE_FLAG_NONE,
                entry.texture->GetResource(),
                D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE,
                mipLevels,
                arraySize,
                &sliceDesc);
            auto *sliceDescInfo = sliceView->GetDescriptorHandleInfoForTextureManager(Passkey<TextureManager>{});
            if (!sliceDescInfo) {
                continue;
            }
            entry.frameSrvGpuPtrs.push_back(sliceDescInfo->gpuHandle.ptr);
            entry.frameSrvIndices.push_back(sliceDescInfo->index);
            entry.frameViews.push_back(std::move(sliceView));
        }
    } else {
        entry.frameSrvGpuPtrs.push_back(entry.srvGpuPtr);
        entry.frameSrvIndices.push_back(entry.srvIndex);
    }

    // 各サブリソースのフットプリント情報を取得
    D3D12_RESOURCE_DESC texDesc = entry.texture->GetResource()->GetDesc();
    UINT subresourceCount = texDesc.MipLevels * texDesc.DepthOrArraySize;

    std::vector<D3D12_PLACED_SUBRESOURCE_FOOTPRINT> layouts(subresourceCount);
    std::vector<UINT> numRows(subresourceCount);
    std::vector<UINT64> rowSizes(subresourceCount);
    UINT64 requiredSize = 0;
    sDevice->GetCopyableFootprints(&texDesc, 0, subresourceCount, 0, layouts.data(), numRows.data(), rowSizes.data(), &requiredSize);

    // アップロード用リソースを作成
    D3D12_HEAP_PROPERTIES uploadHeap{};
    uploadHeap.Type = D3D12_HEAP_TYPE_UPLOAD;

    D3D12_RESOURCE_DESC uploadDesc{};
    uploadDesc.Dimension = D3D12_RESOURCE_DIMENSION_BUFFER;
    uploadDesc.Alignment = 0;
    uploadDesc.Width = requiredSize;
    uploadDesc.Height = 1;
    uploadDesc.DepthOrArraySize = 1;
    uploadDesc.MipLevels = 1;
    uploadDesc.Format = DXGI_FORMAT_UNKNOWN;
    uploadDesc.SampleDesc = { 1, 0 };
    uploadDesc.Layout = D3D12_TEXTURE_LAYOUT_ROW_MAJOR;
    uploadDesc.Flags = D3D12_RESOURCE_FLAG_NONE;

    HRESULT hr = E_FAIL;
    hr = sDevice->CreateCommittedResource(
        &uploadHeap,
        D3D12_HEAP_FLAG_NONE,
        &uploadDesc,
        D3D12_RESOURCE_STATE_GENERIC_READ,
        nullptr,
        IID_PPV_ARGS(entry.upload.GetAddressOf()));
    if (FAILED(hr)) {
        Log(Translation("engine.texture.loading.failed.createupload") + PathToUtf8String(p), LogSeverity::Error);
        return kInvalidHandle;
    }

    // アップロード用リソースにデータを書き込み
    {
        void* mapped = nullptr;
        D3D12_RANGE range{ 0, 0 };
        hr = entry.upload->Map(0, &range, &mapped);
        if (FAILED(hr) || !mapped) {
            Log(Translation("engine.texture.loading.failed.map") + PathToUtf8String(p), LogSeverity::Error);
            return kInvalidHandle;
        }
        uint8_t* dstAll = static_cast<uint8_t*>(mapped);
        const UINT arrayCount = texDesc.DepthOrArraySize;
        const UINT mipCount = texDesc.MipLevels;
        for (UINT arraySlice = 0; arraySlice < arrayCount; ++arraySlice) {
            for (UINT mip = 0; mip < mipCount; ++mip) {
                const UINT subresource = D3D12CalcSubresource(mip, arraySlice, 0, mipCount, arrayCount);
                const DirectX::Image* img = mipChain->GetImage(mip, arraySlice, 0);
                if (!img || !img->pixels) continue;
                auto &fp = layouts[subresource].Footprint;
                uint8_t* dst = dstAll + layouts[subresource].Offset;
                for (UINT y = 0; y < numRows[subresource]; ++y) {
                    memcpy(dst + static_cast<size_t>(y) * fp.RowPitch, img->pixels + static_cast<size_t>(y) * img->rowPitch, img->rowPitch);
                }
            }
        }
        entry.upload->Unmap(0, nullptr);
    }

    // GPUへコピー（各サブリソース）
    directXCommon_->ExecuteOneShotCommandsForTextureManager(Passkey<TextureManager>{},
        [&](ID3D12GraphicsCommandList* cl) {
            for (UINT i = 0; i < subresourceCount; ++i) {
                D3D12_TEXTURE_COPY_LOCATION dstLoc{};
                dstLoc.pResource = entry.texture->GetResource();
                dstLoc.Type = D3D12_TEXTURE_COPY_TYPE_SUBRESOURCE_INDEX;
                dstLoc.SubresourceIndex = i;

                D3D12_TEXTURE_COPY_LOCATION srcLoc{};
                srcLoc.pResource = entry.upload.Get();
                srcLoc.Type = D3D12_TEXTURE_COPY_TYPE_PLACED_FOOTPRINT;
                srcLoc.PlacedFootprint = layouts[i];

                cl->CopyTextureRegion(&dstLoc, 0, 0, 0, &srcLoc, nullptr);
            }

            D3D12_RESOURCE_BARRIER barrier{};
            barrier.Type = D3D12_RESOURCE_BARRIER_TYPE_TRANSITION;
            barrier.Flags = D3D12_RESOURCE_BARRIER_FLAG_NONE;
            barrier.Transition.pResource = entry.texture->GetResource();
            barrier.Transition.Subresource = D3D12_RESOURCE_BARRIER_ALL_SUBRESOURCES;
            barrier.Transition.StateBefore = D3D12_RESOURCE_STATE_COPY_DEST;
            barrier.Transition.StateAfter = D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE;
            cl->ResourceBarrier(1, &barrier);
        });

    // アップロード用リソースはもう不要なので解放
    entry.upload.Reset();

    const auto handle = RegisterEntry(std::move(entry));
    if (handle == kInvalidHandle) {
        Log(Translation("engine.texture.loading.failed.register") + PathToUtf8String(p), LogSeverity::Error);
        return kInvalidHandle;
    }

    Log(Translation("engine.texture.loading.succeeded") + PathToUtf8String(p), LogSeverity::Info);
    return handle;
}

DirectX::ScratchImage TextureManager::LoadTextureFromFile(const std::string& filePath) {
    LogScope scope;
    if (filePath.empty()) return DirectX::ScratchImage();

    Log(Translation("engine.texture.loading.start") + filePath, LogSeverity::Info);

    {
        const std::string normalized = NormalizePathSlashes(filePath);
        auto it = sAssetPathToHandle.find(normalized);
        if (it != sAssetPathToHandle.end()) {
            Log(Translation("engine.texture.loading.alreadyloaded") + normalized, LogSeverity::Debug);
            return DirectX::ScratchImage();
        }
    }

    const std::filesystem::path p = Utf8StringToPath(filePath);

    if (!std::filesystem::exists(p)) {
        Log(Translation("engine.texture.loading.failed.notfound") + PathToUtf8String(p), LogSeverity::Warning);
        return DirectX::ScratchImage();
    }
    if (!HasSupportedImageExtension(p)) {
        Log(Translation("engine.texture.loading.failed.unsupported") + PathToUtf8String(p), LogSeverity::Warning);
        return DirectX::ScratchImage();
    }

    if (!directXCommon_ || !sDevice || !sSrvHeap) {
        Log(Translation("engine.texture.loading.failed.notinitialized") + PathToUtf8String(p), LogSeverity::Error);
        return DirectX::ScratchImage();
    }

    DirectX::TexMetadata meta{};
    DirectX::ScratchImage scratch;

    const std::wstring wpath = ConvertString(PathToUtf8String(p));

    HRESULT hr = E_FAIL;
    const std::string ext = ToLower(p.extension().string());
    if (ext == ".dds") {
        hr = DirectX::LoadFromDDSFile(wpath.c_str(), DirectX::DDS_FLAGS_NONE, &meta, scratch);
    } else if (ext == ".tga") {
        hr = DirectX::LoadFromTGAFile(wpath.c_str(), &meta, scratch);
    } else if (ext == ".hdr") {
        hr = DirectX::LoadFromHDRFile(wpath.c_str(), &meta, scratch);
    } else {
        hr = DirectX::LoadFromWICFile(wpath.c_str(), DirectX::WIC_FLAGS_FORCE_RGB, &meta, scratch);
    }
    if (FAILED(hr)) {
        Log(Translation("engine.texture.loading.failed.decode") + PathToUtf8String(p), LogSeverity::Warning);
        return DirectX::ScratchImage();
    }

    DXGI_FORMAT dstFormat;
    if (ext == ".hdr" || ext == ".tga") {
        dstFormat = DXGI_FORMAT_R8G8B8A8_UNORM;
        if (ext == ".hdr") {
            dstFormat = DXGI_FORMAT_R32G32B32A32_FLOAT;
        }
    } else if (ext == ".dds") {
        dstFormat = meta.format;
    } else {
        dstFormat = DXGI_FORMAT_R8G8B8A8_UNORM;
    }

    return ConvertAndGenerateMips(std::move(scratch), meta, dstFormat);
}

DirectX::ScratchImage TextureManager::LoadTextureFromMemory(const void* data, size_t dataSize) {
    if (!data || dataSize == 0) return DirectX::ScratchImage();
    if (!directXCommon_ || !sDevice || !sSrvHeap) return DirectX::ScratchImage();

    DirectX::TexMetadata meta{};
    DirectX::ScratchImage scratch;
    // glTF等の埋め込みテクスチャは常にWICが認識できる圧縮形式（PNG/JPEG）のため、
    // ファイル拡張子を問わずWICメモリデコードのみで対応する
    HRESULT hr = DirectX::LoadFromWICMemory(static_cast<const uint8_t*>(data), dataSize, DirectX::WIC_FLAGS_FORCE_RGB, &meta, scratch);
    if (FAILED(hr)) {
        Log(Translation("engine.texture.loading.failed.decode") + std::string("(memory)"), LogSeverity::Warning);
        return DirectX::ScratchImage();
    }

    return ConvertAndGenerateMips(std::move(scratch), meta, DXGI_FORMAT_R8G8B8A8_UNORM);
}

TextureManager::TextureHandle TextureManager::RegisterTextureFromMemory(const std::string& registerPath, const void* data, size_t dataSize) {
    if (registerPath.empty()) return kInvalidHandle;

    const auto existing = GetTextureFromAssetPath(MakeAssetRelativePath(assetsRootPath_, registerPath));
    if (existing != kInvalidHandle) return existing;

    DirectX::ScratchImage mipChain = LoadTextureFromMemory(data, dataSize);
    if (mipChain.GetImageCount() == 0) return kInvalidHandle;

    mipMapContainer_.AddMipMap(registerPath, std::move(mipChain));
    return LoadTexture(registerPath);
}

#if defined(USE_IMGUI)
TextureManager::TextureHandle TextureManager::LoadTextureDynamic(const std::string &filePath) {
    if (!sActiveInstance) return kInvalidHandle;

    const auto existing = GetTextureFromAssetPath(MakeAssetRelativePath(sActiveInstance->assetsRootPath_, filePath));
    if (existing != kInvalidHandle) return existing;

    DirectX::ScratchImage mipChain = sActiveInstance->LoadTextureFromFile(filePath);
    if (mipChain.GetImageCount() == 0) return kInvalidHandle;

    sActiveInstance->mipMapContainer_.AddMipMap(filePath, std::move(mipChain));
    return sActiveInstance->LoadTexture(filePath);
}
#endif

TextureManager::TextureHandle TextureManager::GetTexture(TextureHandle handle) {
    LogScope scope;
    if (handle == kInvalidHandle) return kInvalidHandle;
    if (sTextures.find(handle) == sTextures.end()) return kInvalidHandle;
    return handle;
}

TextureManager::TextureHandle TextureManager::GetTextureFromFileName(const std::string& fileName) {
    LogScope scope;
    auto it = sFileNameToHandle.find(fileName);
    if (it == sFileNameToHandle.end()) return kInvalidHandle;
    return it->second;
}

TextureManager::TextureHandle TextureManager::GetTextureFromAssetPath(const std::string& assetPath) {
    LogScope scope;
    auto it = sAssetPathToHandle.find(NormalizePathSlashes(assetPath));
    if (it == sAssetPathToHandle.end()) return kInvalidHandle;
    return it->second;
}

std::string TextureManager::GetTextureFileName(TextureHandle handle) {
    LogScope scope;
    if (handle == kInvalidHandle) return {};
    auto it = sTextures.find(handle);
    if (it == sTextures.end()) return {};
    return it->second.fileName;
}

std::string TextureManager::GetTextureAssetPath(TextureHandle handle) {
    LogScope scope;
    if (handle == kInvalidHandle) return {};
    auto it = sTextures.find(handle);
    if (it == sTextures.end()) return {};
    return it->second.assetPath;
}

bool TextureManager::RenameTexture(const std::string &oldAssetPath, const std::string &newAssetPath) {
    LogScope scope;
    const std::string normalizedOld = NormalizePathSlashes(oldAssetPath);
    auto pathIt = sAssetPathToHandle.find(normalizedOld);
    if (pathIt == sAssetPathToHandle.end()) return false;
    const Handle handle = pathIt->second;
    auto entryIt = sTextures.find(handle);
    if (entryIt == sTextures.end()) return false;

    TextureEntry &entry = entryIt->second;
    sAssetPathToHandle.erase(pathIt);
    sFileNameToHandle.erase(entry.fileName);

    const std::string normalizedNew = NormalizePathSlashes(newAssetPath);
    entry.assetPath = normalizedNew;
    entry.fileName = PathToUtf8String(Utf8StringToPath(normalizedNew).filename());
    entry.fullPath = ProjectPaths::AssetsRoot() + "/" + normalizedNew;

    sAssetPathToHandle[normalizedNew] = handle;
    sFileNameToHandle[entry.fileName] = handle;
    return true;
}

std::vector<TextureManager::TextureListEntry> TextureManager::GetLoadedTextureListEntries() {
    LogScope scope;
    std::vector<TextureListEntry> out;
    out.reserve(sTextures.size());

    for (const auto &kv : sTextures) {
        const auto &t = kv.second;
        TextureListEntry e;
        e.handle = kv.first;
        e.fileName = t.fileName;
        e.assetPath = t.assetPath;
        if (t.external) {
            e.width = t.external->GetWidth();
            e.height = t.external->GetHeight();
            e.srvGpuPtr = t.external->GetSrvHandle().ptr;
        } else {
            e.width = t.width;
            e.height = t.height;
            e.srvGpuPtr = t.srvGpuPtr;
        }
        out.push_back(std::move(e));
    }

    std::sort(out.begin(), out.end(), [](const TextureListEntry &a, const TextureListEntry &b) {
        return a.assetPath < b.assetPath;
        });
    return out;
}

#if defined(USE_IMGUI)
namespace {
ImTextureID ToImGuiTextureIdFromGpuPtr(UINT64 gpuPtr) {
    return (ImTextureID)(uintptr_t)gpuPtr;
}
} // namespace

void TextureManager::ShowImGuiLoadedTexturesWindow() {
    ImGui::Begin(TranslationLabel("editor.texturemanager.window"));

    const auto entries = GetImGuiTextureListEntries();
    ImGui::Text(TranslationC("editor.texturemanager.loaded_textures_d"), static_cast<int>(entries.size()));

    static ImGuiTextFilter filter;
    filter.Draw("Filter");

    static TextureManager::TextureListEntry sSelectedTexture{};
    static bool sShowTextureViewer = false;

    ImGui::Separator();

    if (ImGui::BeginTable("##TextureList", 5,
            ImGuiTableFlags_Borders | ImGuiTableFlags_RowBg | ImGuiTableFlags_ScrollY,
            ImVec2(0, 300))) {
        ImGui::TableSetupColumn("Handle", ImGuiTableColumnFlags_WidthFixed, 80);
        ImGui::TableSetupColumn("FileName");
        ImGui::TableSetupColumn("AssetPath");
        ImGui::TableSetupColumn("Size", ImGuiTableColumnFlags_WidthFixed, 90);
        ImGui::TableSetupColumn("Preview", ImGuiTableColumnFlags_WidthFixed, 80);
        ImGui::TableHeadersRow();

        for (const auto& e : entries) {
            if (filter.IsActive()) {
                if (!filter.PassFilter(e.fileName.c_str()) && !filter.PassFilter(e.assetPath.c_str())) {
                    continue;
                }
            }

            ImGui::TableNextRow();

            ImGui::TableSetColumnIndex(0);
            ImGui::Text("%u", e.handle);

            ImGui::TableSetColumnIndex(1);
            ImGui::TextUnformatted(e.fileName.c_str());

            ImGui::TableSetColumnIndex(2);
            ImGui::TextUnformatted(e.assetPath.c_str());

            ImGui::TableSetColumnIndex(3);
            ImGui::Text(TranslationC("editor.texturemanager.ux_u"), e.width, e.height);

            ImGui::TableSetColumnIndex(4);
            if (e.srvGpuPtr != 0) {
                ImGui::PushID(static_cast<int>(e.handle));
                const auto texId = ToImGuiTextureIdFromGpuPtr(e.srvGpuPtr);
                if (ImGui::ImageButton("##Preview", texId, ImVec2(64, 64))) {
                    sSelectedTexture = e;
                    sShowTextureViewer = true;
                }
                ImGui::PopID();
            } else {
                ImGui::TextUnformatted("-");
            }
        }

        ImGui::EndTable();
    }

    ImGui::End();

    if (sShowTextureViewer) {
        if (ImGui::Begin(TranslationLabel("editor.texturemanager.viewer.window"), &sShowTextureViewer)) {
            if (sSelectedTexture.srvGpuPtr != 0) {
                ImGui::Text(TranslationC("editor.texturemanager.handle_u"), sSelectedTexture.handle);
                ImGui::TextUnformatted(sSelectedTexture.assetPath.c_str());
                ImGui::Separator();

                int selectedFrame = 0;
                auto it = sTextures.find(sSelectedTexture.handle);
                if (it != sTextures.end()) {
                    auto &entry = it->second;
                    static TextureManager::TextureHandle sLastHandle = TextureManager::kInvalidHandle;
                    static int sSelectedFrame = 0;
                    if (sLastHandle != sSelectedTexture.handle) {
                        sSelectedFrame = 0;
                        sLastHandle = sSelectedTexture.handle;
                    }

                    if (entry.frameCount > 1) {
                        std::vector<std::string> labels;
                        labels.reserve(entry.frameCount);
                        std::vector<const char*> labelPtrs;
                        labelPtrs.reserve(entry.frameCount);
                        for (UINT i = 0; i < entry.frameCount; ++i) {
                            labels.push_back(std::to_string(i));
                        }
                        for (const auto &label : labels) {
                            labelPtrs.push_back(label.c_str());
                        }
                        ImGui::Combo(TranslationLabel("editor.texturemanager.frame"), &sSelectedFrame, labelPtrs.data(), static_cast<int>(labelPtrs.size()));
                    }

                    if (sSelectedFrame < 0) sSelectedFrame = 0;
                    if (static_cast<size_t>(sSelectedFrame) >= entry.frameSrvGpuPtrs.size()) sSelectedFrame = 0;
                    selectedFrame = sSelectedFrame;

                    ImVec2 avail = ImGui::GetContentRegionAvail();
                    const float w = static_cast<float>(sSelectedTexture.width);
                    const float h = static_cast<float>(sSelectedTexture.height);
                    ImVec2 drawSize = avail;
                    if (w > 0.0f && h > 0.0f) {
                        const float sx = avail.x / w;
                        const float sy = avail.y / h;
                        const float s = (sx < sy) ? sx : sy;
                        drawSize = ImVec2(w * s, h * s);
                    }

                    if (!entry.frameSrvGpuPtrs.empty()) {
                        const UINT64 gpuPtr = entry.frameSrvGpuPtrs[static_cast<size_t>(selectedFrame)];
                        ImGui::Image(ToImGuiTextureIdFromGpuPtr(gpuPtr), drawSize);
                    } else {
                        ImGui::Image(ToImGuiTextureIdFromGpuPtr(sSelectedTexture.srvGpuPtr), drawSize);
                    }
                } else {
                    ImVec2 avail = ImGui::GetContentRegionAvail();
                    const float w = static_cast<float>(sSelectedTexture.width);
                    const float h = static_cast<float>(sSelectedTexture.height);
                    ImVec2 drawSize = avail;
                    if (w > 0.0f && h > 0.0f) {
                        const float sx = avail.x / w;
                        const float sy = avail.y / h;
                        const float s = (sx < sy) ? sx : sy;
                        drawSize = ImVec2(w * s, h * s);
                    }

                    ImGui::Image(ToImGuiTextureIdFromGpuPtr(sSelectedTexture.srvGpuPtr), drawSize);
                }
            } else {
                ImGui::TextUnformatted(TranslationC("editor.texturemanager.no_texture_selected"));
            }
        }
        ImGui::End();
    }
}
#endif

#if defined(USE_IMGUI)
std::vector<TextureManager::TextureHandle> TextureManager::GetAllImGuiTextures() {
    LogScope scope;
    std::vector<TextureHandle> out;
    out.reserve(sTextures.size());
    for (const auto &kv : sTextures) out.push_back(kv.first);
    return out;
}

std::vector<TextureManager::TextureListEntry> TextureManager::GetImGuiTextureListEntries() {
    return GetLoadedTextureListEntries();
}
#endif

} // namespace KashipanEngine
