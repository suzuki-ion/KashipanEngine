#include "TextureManager.h"
#include "Assets/CaseInsensitive.h"

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

    std::unique_ptr<ShaderResourceResource> texture;
    Microsoft::WRL::ComPtr<ID3D12Resource> upload;

    UINT width = 0;
    UINT height = 0;
    DXGI_FORMAT format = DXGI_FORMAT_UNKNOWN;
    UINT64 srvGpuPtr = 0;
    UINT srvIndex = 0;
    UINT mipLevels = 1;
};

std::unordered_map<Handle, TextureEntry> sTextures;
FileMap<Handle> sFileNameToHandle;
FileMap<Handle> sAssetPathToHandle;

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
    h.ptr = it->second.srvGpuPtr;
    return h;
}

std::uint32_t TextureManager::TextureView::GetWidth() const noexcept {
    if (handle_ == kInvalidHandle) return 0;
    auto it = sTextures.find(handle_);
    if (it == sTextures.end()) return 0;
    return static_cast<std::uint32_t>(it->second.width);
}

std::uint32_t TextureManager::TextureView::GetHeight() const noexcept {
    if (handle_ == kInvalidHandle) return 0;
    auto it = sTextures.find(handle_);
    if (it == sTextures.end()) return 0;
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
    h.ptr = it->second.srvGpuPtr;
    if (h.ptr == 0) return false;

    return shaderBinder->Bind(nameKey, h);
}

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
    DXGI_FORMAT dstFormat;
    if (ext == ".dds" || ext == ".hdr" || ext == ".tga") {
        dstFormat = DXGI_FORMAT_R8G8B8A8_UNORM;
        if (ext == ".hdr") {
            dstFormat = DXGI_FORMAT_R32G32B32A32_FLOAT;
        }
    } else {
        dstFormat = DXGI_FORMAT_R8G8B8A8_UNORM;
    }

    if (meta.format != dstFormat) {
        hr = DirectX::Convert(scratch.GetImages(), scratch.GetImageCount(), scratch.GetMetadata(), dstFormat, DirectX::TEX_FILTER_SRGB, DirectX::TEX_THRESHOLD_DEFAULT, converted);
        if (FAILED(hr)) return kInvalidHandle;
    }
    const DirectX::ScratchImage &finalImage = (meta.format == dstFormat) ? scratch : converted;

    // ミップマップ生成
    DirectX::ScratchImage mipChain;
    hr = DirectX::GenerateMipMaps(finalImage.GetImages(), finalImage.GetImageCount(), finalImage.GetMetadata(), DirectX::TEX_FILTER_SRGB, 0, mipChain);
    if (FAILED(hr)) {
        // ミップマップ生成に失敗した場合は元画像をそのまま使う
        const DirectX::Image *baseImg = finalImage.GetImages();
        if (!baseImg || !baseImg->pixels) {
            return kInvalidHandle;
        }
        hr = mipChain.InitializeFromImage(*baseImg);
        if (FAILED(hr)) {
            return kInvalidHandle;
        }
    }

    const DirectX::Image *img0 = mipChain.GetImages();
    if (!img0 || !img0->pixels) {
        return kInvalidHandle;
    }

    const auto &mmeta = mipChain.GetMetadata();
    UINT mipLevels = static_cast<UINT>(mmeta.mipLevels);

    TextureEntry entry{};
    entry.fullPath = NormalizePathSlashes(p.string());
    entry.assetPath = MakeAssetRelativePath(assetsRootPath_, entry.fullPath);
    entry.fileName = p.filename().string();
    entry.width = static_cast<UINT>(img0->width);
    entry.height = static_cast<UINT>(img0->height);
    entry.format = mmeta.format;
    entry.mipLevels = static_cast<UINT>(mmeta.mipLevels);

    // GPU側テクスチャ + SRV を Resources 経由で作成（COPY_DEST から開始してこの後のコピーに備える）
    entry.texture = std::make_unique<ShaderResourceResource>(
        entry.width,
        entry.height,
        entry.format,
        D3D12_RESOURCE_FLAG_NONE,
        nullptr,
        D3D12_RESOURCE_STATE_COPY_DEST,
        mipLevels);

    {
        auto *desc = entry.texture->GetDescriptorHandleInfoForTextureManager(Passkey<TextureManager>{});
        if (!desc) {
            Log(Translation("engine.texture.loading.failed.createresource") + p.string(), LogSeverity::Error);
            return kInvalidHandle;
        }
        entry.srvGpuPtr = desc->gpuHandle.ptr;
        entry.srvIndex = desc->index;
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

    // アップロード用リソースにデータを書き込み
    {
        void* mapped = nullptr;
        D3D12_RANGE range{ 0, 0 };
        hr = entry.upload->Map(0, &range, &mapped);
        if (FAILED(hr) || !mapped) {
            Log(Translation("engine.texture.loading.failed.map") + p.string(), LogSeverity::Error);
            return kInvalidHandle;
        }
        uint8_t* dstAll = static_cast<uint8_t*>(mapped);
        for (UINT i = 0; i < subresourceCount; ++i) {
            const DirectX::Image* img = mipChain.GetImage(i, 0, 0);
            if (!img || !img->pixels) continue;
            auto &fp = layouts[i].Footprint;
            uint8_t* dst = dstAll + layouts[i].Offset;
            for (UINT y = 0; y < numRows[i]; ++y) {
                memcpy(dst + static_cast<size_t>(y) * fp.RowPitch, img->pixels + static_cast<size_t>(y) * img->rowPitch, img->rowPitch);
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
namespace {
ImTextureID ToImGuiTextureIdFromGpuPtr(UINT64 gpuPtr) {
    return (ImTextureID)(uintptr_t)gpuPtr;
}
} // namespace

void TextureManager::ShowImGuiLoadedTexturesWindow() {
    ImGui::Begin("TextureManager - Loaded Textures");

    const auto entries = GetImGuiTextureListEntries();
    ImGui::Text("Loaded Textures: %d", static_cast<int>(entries.size()));

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
            ImGui::Text("%ux%u", e.width, e.height);

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
        if (ImGui::Begin("Texture Viewer", &sShowTextureViewer)) {
            if (sSelectedTexture.srvGpuPtr != 0) {
                ImGui::Text("Handle: %u", sSelectedTexture.handle);
                ImGui::TextUnformatted(sSelectedTexture.assetPath.c_str());
                ImGui::Separator();

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
            } else {
                ImGui::TextUnformatted("No texture selected.");
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
    LogScope scope;
    std::vector<TextureListEntry> out;
    out.reserve(sTextures.size());

    for (const auto &kv : sTextures) {
        const auto &t = kv.second;
        TextureListEntry e;
        e.handle = kv.first;
        e.fileName = t.fileName;
        e.assetPath = t.assetPath;
        e.width = t.width;
        e.height = t.height;
        e.srvGpuPtr = t.srvGpuPtr;
        out.push_back(std::move(e));
    }

    std::sort(out.begin(), out.end(), [](const TextureListEntry &a, const TextureListEntry &b) {
        return a.assetPath < b.assetPath;
        });
    return out;
}
#endif

} // namespace KashipanEngine
