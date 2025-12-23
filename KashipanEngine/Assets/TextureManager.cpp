#include "TextureManager.h"

#include "Core/DirectXCommon.h"
#include "Utilities/Conversion/ConvertString.h"
#include "Utilities/FileIO/Directory.h"

#include <DirectXTex.h>

#include <d3d12.h>
#include <wrl.h>

#include <algorithm>
#include <filesystem>
#include <functional>
#include <unordered_map>
#include <vector>

namespace KashipanEngine {

namespace {

using Handle = TextureManager::TextureHandle;

struct TextureEntry final {
    std::string fullPath;
    std::string assetPath;
    std::string fileName;

    Microsoft::WRL::ComPtr<ID3D12Resource> resource;
    Microsoft::WRL::ComPtr<ID3D12Resource> upload;
    std::unique_ptr<DescriptorHandleInfo> srv;
    UINT width = 0;
    UINT height = 0;
    DXGI_FORMAT format = DXGI_FORMAT_UNKNOWN;
    UINT64 srvGpuPtr = 0;
};

std::unordered_map<Handle, TextureEntry> sTextures;
std::unordered_map<std::string, Handle> sFileNameToHandle;
std::unordered_map<std::string, Handle> sAssetPathToHandle;

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
    std::filesystem::path root(assetsRoot);
    std::filesystem::path full(fullPath);

    std::error_code ec;
    auto rel = std::filesystem::relative(full, root, ec);
    if (ec) {
        return NormalizePathSlashes(full.filename().string());
    }
    return NormalizePathSlashes(rel.string());
}

Handle RegisterEntry(TextureEntry&& entry) {
    if (!entry.srv) return TextureManager::kInvalidHandle;

    // TextureHandle は 0 を無効値とするため、SRV index に +1 した値をハンドルとして返す
    const Handle handle = static_cast<Handle>(entry.srv->index + 1u);

    if (handle == TextureManager::kInvalidHandle) return TextureManager::kInvalidHandle;
    if (sTextures.find(handle) != sTextures.end()) return TextureManager::kInvalidHandle;

    sFileNameToHandle[entry.fileName] = handle;
    sAssetPathToHandle[NormalizePathSlashes(entry.assetPath)] = handle;
    sTextures.emplace(handle, std::move(entry));
    return handle;
}

UINT Align256(UINT v) { return (v + 255u) & ~255u; }

} // namespace

TextureManager::TextureManager(Passkey<GameEngine>, DirectXCommon* directXCommon, const std::string& assetsRootPath)
    : directXCommon_(directXCommon), assetsRootPath_(NormalizePathSlashes(assetsRootPath)) {
    LogScope scope;
    if (directXCommon_) {
        sDevice = directXCommon_->GetDeviceForTextureManager(Passkey<TextureManager>{});
        sSrvHeap = directXCommon_->GetSRVHeapForTextureManager(Passkey<TextureManager>{});
    }
    LoadAllFromAssetsFolder();
}

TextureManager::~TextureManager() {
    LogScope scope;
    sTextures.clear();
    sFileNameToHandle.clear();
    sAssetPathToHandle.clear();
    sSrvHeap = nullptr;
    sDevice = nullptr;
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

    for (const auto& f : files) {
        LoadTexture(f);
    }
}

TextureManager::TextureHandle TextureManager::LoadTexture(const std::string& filePath) {
    LogScope scope;
    if (filePath.empty()) return kInvalidHandle;

    Log(Translation("engine.texture.loading.start") + filePath, LogSeverity::Info);

    {
        const std::string normalized = NormalizePathSlashes(filePath);
        auto it = sAssetPathToHandle.find(normalized);
        if (it != sAssetPathToHandle.end()) {
            Log(Translation("engine.texture.loading.alreadyloaded") + normalized, LogSeverity::Debug);
            return it->second;
        }
    }

    std::filesystem::path p(filePath);
    /*if (!p.is_absolute()) {
        p = std::filesystem::path(assetsRootPath_) / p;
    }*/

    if (!std::filesystem::exists(p)) {
        Log(Translation("engine.texture.loading.failed.notfound") + p.string(), LogSeverity::Warning);
        return kInvalidHandle;
    }
    if (!HasSupportedImageExtension(p)) {
        Log(Translation("engine.texture.loading.failed.unsupported") + p.string(), LogSeverity::Warning);
        return kInvalidHandle;
    }

    if (!directXCommon_ || !sDevice || !sSrvHeap) {
        Log(Translation("engine.texture.loading.failed.notinitialized") + p.string(), LogSeverity::Error);
        return kInvalidHandle;
    }

    DirectX::TexMetadata meta{};
    DirectX::ScratchImage scratch;

    const std::wstring wpath = ConvertString(p.string());

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
        Log(Translation("engine.texture.loading.failed.decode") + p.string(), LogSeverity::Warning);
        return kInvalidHandle;
    }

    DirectX::ScratchImage converted;
    const DXGI_FORMAT dstFormat = DXGI_FORMAT_R8G8B8A8_UNORM;
    if (meta.format != dstFormat) {
        hr = DirectX::Convert(*scratch.GetImage(0, 0, 0), dstFormat, DirectX::TEX_FILTER_DEFAULT, DirectX::TEX_THRESHOLD_DEFAULT, converted);
        if (FAILED(hr)) return kInvalidHandle;
    }
    const DirectX::ScratchImage& finalImage = (meta.format == dstFormat) ? scratch : converted;

    const DirectX::Image* img0 = finalImage.GetImage(0, 0, 0);
    if (!img0 || !img0->pixels) {
        return kInvalidHandle;
    }

    TextureEntry entry{};
    entry.fullPath = NormalizePathSlashes(p.string());
    entry.assetPath = MakeAssetRelativePath(assetsRootPath_, entry.fullPath);
    entry.fileName = p.filename().string();
    entry.width = static_cast<UINT>(img0->width);
    entry.height = static_cast<UINT>(img0->height);
    entry.format = dstFormat;

    entry.srv = sSrvHeap->AllocateDescriptorHandle();

    // テクスチャリソース作成
    D3D12_RESOURCE_DESC texDesc{};
    texDesc.Dimension = D3D12_RESOURCE_DIMENSION_TEXTURE2D;
    texDesc.Alignment = 0;
    texDesc.Width = static_cast<UINT64>(entry.width);
    texDesc.Height = entry.height;
    texDesc.DepthOrArraySize = 1;
    texDesc.MipLevels = 1;
    texDesc.Format = entry.format;
    texDesc.SampleDesc = { 1, 0 };
    texDesc.Layout = D3D12_TEXTURE_LAYOUT_UNKNOWN;
    texDesc.Flags = D3D12_RESOURCE_FLAG_NONE;

    D3D12_HEAP_PROPERTIES defaultHeap{};
    defaultHeap.Type = D3D12_HEAP_TYPE_DEFAULT;

    hr = sDevice->CreateCommittedResource(
        &defaultHeap,
        D3D12_HEAP_FLAG_NONE,
        &texDesc,
        D3D12_RESOURCE_STATE_COPY_DEST,
        nullptr,
        IID_PPV_ARGS(entry.resource.GetAddressOf()));
    if (FAILED(hr)) {
        Log(Translation("engine.texture.loading.failed.createresource") + p.string(), LogSeverity::Error);
        return kInvalidHandle;
    }

    // アップロード用バッファ作成
    const UINT rowPitch = Align256(static_cast<UINT>(img0->rowPitch));
    const UINT64 uploadSize = static_cast<UINT64>(rowPitch) * static_cast<UINT64>(entry.height);

    D3D12_HEAP_PROPERTIES uploadHeap{};
    uploadHeap.Type = D3D12_HEAP_TYPE_UPLOAD;

    D3D12_RESOURCE_DESC uploadDesc{};
    uploadDesc.Dimension = D3D12_RESOURCE_DIMENSION_BUFFER;
    uploadDesc.Alignment = 0;
    uploadDesc.Width = uploadSize;
    uploadDesc.Height = 1;
    uploadDesc.DepthOrArraySize = 1;
    uploadDesc.MipLevels = 1;
    uploadDesc.Format = DXGI_FORMAT_UNKNOWN;
    uploadDesc.SampleDesc = { 1, 0 };
    uploadDesc.Layout = D3D12_TEXTURE_LAYOUT_ROW_MAJOR;
    uploadDesc.Flags = D3D12_RESOURCE_FLAG_NONE;

    hr = sDevice->CreateCommittedResource(
        &uploadHeap,
        D3D12_HEAP_FLAG_NONE,
        &uploadDesc,
        D3D12_RESOURCE_STATE_GENERIC_READ,
        nullptr,
        IID_PPV_ARGS(entry.upload.GetAddressOf()));
    if (FAILED(hr)) {
        Log(Translation("engine.texture.loading.failed.createupload") + p.string(), LogSeverity::Error);
        return kInvalidHandle;
    }

    // アップロード用バッファにデータ転送
    {
        void* mapped = nullptr;
        D3D12_RANGE range{ 0, 0 };
        hr = entry.upload->Map(0, &range, &mapped);
        if (FAILED(hr) || !mapped) {
            Log(Translation("engine.texture.loading.failed.map") + p.string(), LogSeverity::Error);
            return kInvalidHandle;
        }
        auto* dst = static_cast<uint8_t*>(mapped);
        const uint8_t* src = img0->pixels;
        for (UINT y = 0; y < entry.height; ++y) {
            memcpy(dst + static_cast<size_t>(y) * rowPitch, src + static_cast<size_t>(y) * img0->rowPitch, img0->rowPitch);
        }
        entry.upload->Unmap(0, nullptr);
    }

    // GPUへコピー
    directXCommon_->ExecuteOneShotCommandsForTextureManager(Passkey<TextureManager>{},
        [&](ID3D12GraphicsCommandList* cl) {
            D3D12_TEXTURE_COPY_LOCATION dstLoc{};
            dstLoc.pResource = entry.resource.Get();
            dstLoc.Type = D3D12_TEXTURE_COPY_TYPE_SUBRESOURCE_INDEX;
            dstLoc.SubresourceIndex = 0;

            D3D12_TEXTURE_COPY_LOCATION srcLoc{};
            srcLoc.pResource = entry.upload.Get();
            srcLoc.Type = D3D12_TEXTURE_COPY_TYPE_PLACED_FOOTPRINT;
            srcLoc.PlacedFootprint.Offset = 0;
            srcLoc.PlacedFootprint.Footprint.Format = entry.format;
            srcLoc.PlacedFootprint.Footprint.Width = entry.width;
            srcLoc.PlacedFootprint.Footprint.Height = entry.height;
            srcLoc.PlacedFootprint.Footprint.Depth = 1;
            srcLoc.PlacedFootprint.Footprint.RowPitch = rowPitch;

            cl->CopyTextureRegion(&dstLoc, 0, 0, 0, &srcLoc, nullptr);

            D3D12_RESOURCE_BARRIER barrier{};
            barrier.Type = D3D12_RESOURCE_BARRIER_TYPE_TRANSITION;
            barrier.Flags = D3D12_RESOURCE_BARRIER_FLAG_NONE;
            barrier.Transition.pResource = entry.resource.Get();
            barrier.Transition.Subresource = D3D12_RESOURCE_BARRIER_ALL_SUBRESOURCES;
            barrier.Transition.StateBefore = D3D12_RESOURCE_STATE_COPY_DEST;
            barrier.Transition.StateAfter = D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE;
            cl->ResourceBarrier(1, &barrier);
        });

    // SRV 作成
    D3D12_SHADER_RESOURCE_VIEW_DESC srvDesc{};
    srvDesc.Format = entry.format;
    srvDesc.ViewDimension = D3D12_SRV_DIMENSION_TEXTURE2D;
    srvDesc.Shader4ComponentMapping = D3D12_DEFAULT_SHADER_4_COMPONENT_MAPPING;
    srvDesc.Texture2D.MipLevels = 1;
    srvDesc.Texture2D.MostDetailedMip = 0;
    srvDesc.Texture2D.ResourceMinLODClamp = 0.0f;

    sDevice->CreateShaderResourceView(entry.resource.Get(), &srvDesc, entry.srv->cpuHandle);
    entry.srvGpuPtr = entry.srv->gpuHandle.ptr;

    // アップロード用リソースはもう不要なので解放
    entry.upload.Reset();

    const auto handle = RegisterEntry(std::move(entry));
    if (handle == kInvalidHandle) {
        Log(Translation("engine.texture.loading.failed.register") + p.string(), LogSeverity::Error);
        return kInvalidHandle;
    }

    Log(Translation("engine.texture.loading.succeeded") + p.string(), LogSeverity::Info);
    return handle;
}

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

#if defined(USE_IMGUI)
std::vector<TextureManager::TextureHandle> TextureManager::GetAllImGuiTextures() {
    LogScope scope;
    std::vector<TextureHandle> out;
    out.reserve(sTextures.size());
    for (const auto& kv : sTextures) {
        out.push_back(kv.first);
    }
    return out;
}

std::vector<TextureManager::TextureListEntry> TextureManager::GetImGuiTextureListEntries() {
    LogScope scope;
    std::vector<TextureListEntry> out;
    out.reserve(sTextures.size());

    for (const auto& kv : sTextures) {
        const auto& t = kv.second;
        TextureListEntry e;
        e.handle = kv.first;
        e.fileName = t.fileName;
        e.assetPath = t.assetPath;
        e.width = t.width;
        e.height = t.height;
        e.srvGpuPtr = t.srvGpuPtr;
        out.push_back(std::move(e));
    }

    std::sort(out.begin(), out.end(), [](const TextureListEntry& a, const TextureListEntry& b) {
        return a.assetPath < b.assetPath;
    });

    return out;
}
#endif

} // namespace KashipanEngine
