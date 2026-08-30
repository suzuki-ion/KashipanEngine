#include "GifTexture.h"

#include <cstring>

#include "Core/DirectXCommon.h"
#include "Graphics/Resources/ShaderResourceResource.h"
#include "Debug/Logger.h"
#include "Utilities/Translation.h"

namespace KashipanEngine {

namespace {

/// @brief D3D12_HEAP_TYPE_UPLOADのバッファリソースを確保し、そのまま永続Mapする
bool CreatePersistentUploadBuffer(ID3D12Device *device, UINT64 sizeBytes, Microsoft::WRL::ComPtr<ID3D12Resource> &outResource, void *&outMapped) {
    D3D12_HEAP_PROPERTIES uploadHeap{};
    uploadHeap.Type = D3D12_HEAP_TYPE_UPLOAD;

    D3D12_RESOURCE_DESC uploadDesc{};
    uploadDesc.Dimension = D3D12_RESOURCE_DIMENSION_BUFFER;
    uploadDesc.Width = sizeBytes;
    uploadDesc.Height = 1;
    uploadDesc.DepthOrArraySize = 1;
    uploadDesc.MipLevels = 1;
    uploadDesc.Format = DXGI_FORMAT_UNKNOWN;
    uploadDesc.SampleDesc = { 1, 0 };
    uploadDesc.Layout = D3D12_TEXTURE_LAYOUT_ROW_MAJOR;
    uploadDesc.Flags = D3D12_RESOURCE_FLAG_NONE;

    HRESULT hr = device->CreateCommittedResource(&uploadHeap, D3D12_HEAP_FLAG_NONE, &uploadDesc,
        D3D12_RESOURCE_STATE_GENERIC_READ, nullptr, IID_PPV_ARGS(outResource.GetAddressOf()));
    if (FAILED(hr) || !outResource) return false;

    D3D12_RANGE range{ 0, 0 };
    hr = outResource->Map(0, &range, &outMapped);
    return SUCCEEDED(hr) && outMapped != nullptr;
}

} // namespace

GifTexture::GifTexture(DirectXCommon *directXCommon, std::uint32_t width, std::uint32_t height)
    : directXCommon_(directXCommon), width_(width), height_(height) {
    LogScope scope;

    if (!directXCommon_ || width_ == 0 || height_ == 0) {
        Log(Translation("engine.gif.texture.initialize.invalidsize"), LogSeverity::Error);
        return;
    }

    auto *device = directXCommon_->GetDeviceForGifTexture(Passkey<GifTexture>{});
    if (!device) {
        Log(Translation("engine.gif.texture.initialize.failed"), LogSeverity::Error);
        return;
    }

    texture_ = std::make_unique<ShaderResourceResource>(
        width_, height_, DXGI_FORMAT_R8G8B8A8_UNORM, D3D12_RESOURCE_FLAG_NONE, nullptr,
        D3D12_RESOURCE_STATE_COPY_DEST, 1, 1, nullptr);
    if (!texture_->GetResource()) {
        Log(Translation("engine.gif.texture.initialize.failed"), LogSeverity::Error);
        return;
    }

    D3D12_RESOURCE_DESC desc = texture_->GetResource()->GetDesc();
    UINT64 requiredSize = 0;
    UINT64 rowSizeInBytes = 0;
    device->GetCopyableFootprints(&desc, 0, 1, 0, &layout_, &numRows_, &rowSizeInBytes, &requiredSize);

    if (!CreatePersistentUploadBuffer(device, layout_.Footprint.RowPitch * numRows_, upload_, mapped_)) {
        Log(Translation("engine.gif.texture.initialize.failed"), LogSeverity::Error);
        return;
    }

    valid_ = true;
}

GifTexture::~GifTexture() = default;

bool GifTexture::UploadFrame(const std::uint8_t *rgba, std::size_t dataSize) {
    if (!valid_ || !rgba) return false;

    const std::size_t expectedSize = static_cast<std::size_t>(width_) * height_ * 4;
    if (dataSize < expectedSize) {
        Log(Translation("engine.gif.texture.upload.failed.size"), LogSeverity::Warning);
        return false;
    }

    auto *dst = static_cast<std::uint8_t *>(mapped_);
    const std::size_t srcRowBytes = static_cast<std::size_t>(width_) * 4;
    for (UINT y = 0; y < numRows_; ++y) {
        std::memcpy(dst + static_cast<std::size_t>(y) * layout_.Footprint.RowPitch,
            rgba + static_cast<std::size_t>(y) * srcRowBytes, srcRowBytes);
    }

    ShaderResourceResource *texture = texture_.get();
    ID3D12Resource *upload = upload_.Get();
    const D3D12_PLACED_SUBRESOURCE_FOOTPRINT &layout = layout_;

    directXCommon_->ExecuteOneShotCommandsForGifTexture(Passkey<GifTexture>{}, [&](ID3D12GraphicsCommandList *cl) {
        texture->SetCommandList(cl);
        texture->TransitionTo(D3D12_RESOURCE_STATE_COPY_DEST);

        D3D12_TEXTURE_COPY_LOCATION dstLoc{};
        dstLoc.pResource = texture->GetResource();
        dstLoc.Type = D3D12_TEXTURE_COPY_TYPE_SUBRESOURCE_INDEX;
        dstLoc.SubresourceIndex = 0;
        D3D12_TEXTURE_COPY_LOCATION srcLoc{};
        srcLoc.pResource = upload;
        srcLoc.Type = D3D12_TEXTURE_COPY_TYPE_PLACED_FOOTPRINT;
        srcLoc.PlacedFootprint = layout;
        cl->CopyTextureRegion(&dstLoc, 0, 0, 0, &srcLoc, nullptr);

        texture->TransitionTo(D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE);
    });

    return true;
}

D3D12_GPU_DESCRIPTOR_HANDLE GifTexture::GetSrvHandle() const noexcept {
    return texture_ ? texture_->GetGPUDescriptorHandle() : D3D12_GPU_DESCRIPTOR_HANDLE{};
}

} // namespace KashipanEngine
