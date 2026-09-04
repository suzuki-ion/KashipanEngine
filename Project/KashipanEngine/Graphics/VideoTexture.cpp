#include "VideoTexture.h"

#include <cstring>

#include "Core/DirectXCommon.h"
#include "Graphics/Resources/ShaderResourceResource.h"
#include "Graphics/Resources/UnorderedAccessResource.h"
#include "Debug/Logger.h"
#include "Utilities/Translation.h"

namespace KashipanEngine {

class VideoTextureRgbaView final : public IShaderTexture {
public:
    explicit VideoTextureRgbaView(VideoTexture *owner) : owner_(owner) {}
    D3D12_GPU_DESCRIPTOR_HANDLE GetSrvHandle() const noexcept override { return owner_->GetRgbaSrvHandle(); }
    std::uint32_t GetWidth() const noexcept override { return owner_->GetWidth(); }
    std::uint32_t GetHeight() const noexcept override { return owner_->GetHeight(); }

private:
    VideoTexture *owner_ = nullptr;
};

namespace {

/// @brief D3D12_HEAP_TYPE_UPLOADのバッファリソースを確保し、そのまま永続Mapする
bool CreatePersistentUploadBuffer(ID3D12Device *device, UINT64 sizeBytes, Microsoft::WRL::ComPtr<ID3D12Resource> &outResource, void *&outMapped) {
    LogScope scope;
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

VideoTexture::VideoTexture(DirectXCommon *directXCommon, std::uint32_t width, std::uint32_t height)
    : directXCommon_(directXCommon), width_(width), height_(height) {
    LogScope scope;

    rgbaView_ = std::make_unique<VideoTextureRgbaView>(this);

    if (!directXCommon_ || width_ == 0 || height_ == 0) {
        Log(Translation("engine.video.texture.initialize.invalidsize"), LogSeverity::Error);
        return;
    }

    // NV12は4:2:0のクロマサブサンプリングのため、UV平面は幅高さともに半分（奇数の場合は切り上げ）
    chromaWidth_ = (width_ + 1) / 2;
    chromaHeight_ = (height_ + 1) / 2;

    for (auto &slot : yuvSlots_) {
        if (!InitializeYuvSlot(slot)) {
            Log(Translation("engine.video.texture.initialize.failed"), LogSeverity::Error);
            return;
        }
    }

    if (!InitializeRgbaOutput()) {
        Log(Translation("engine.video.texture.initialize.failed"), LogSeverity::Error);
        return;
    }

    valid_ = true;
}

VideoTexture::~VideoTexture() = default;

bool VideoTexture::InitializeYuvSlot(YuvSlot &slot) {
    LogScope scope;
    auto *device = directXCommon_->GetDeviceForVideoTexture(Passkey<VideoTexture>{});
    if (!device) return false;

    slot.lumaTexture = std::make_unique<ShaderResourceResource>(
        width_, height_, DXGI_FORMAT_R8_UNORM, D3D12_RESOURCE_FLAG_NONE, nullptr,
        D3D12_RESOURCE_STATE_COPY_DEST, 1, 1, nullptr);
    if (!slot.lumaTexture->GetResource()) return false;

    slot.chromaTexture = std::make_unique<ShaderResourceResource>(
        chromaWidth_, chromaHeight_, DXGI_FORMAT_R8G8_UNORM, D3D12_RESOURCE_FLAG_NONE, nullptr,
        D3D12_RESOURCE_STATE_COPY_DEST, 1, 1, nullptr);
    if (!slot.chromaTexture->GetResource()) return false;

    // フットプリント(RowPitch等)は全スロットで同一なので、まだ計算していなければここで一度だけ計算しておく
    if (lumaLayout_.Footprint.RowPitch == 0) {
        D3D12_RESOURCE_DESC lumaDesc = slot.lumaTexture->GetResource()->GetDesc();
        UINT64 lumaRequiredSize = 0;
        UINT64 lumaRowSizeInBytes = 0;
        device->GetCopyableFootprints(&lumaDesc, 0, 1, 0, &lumaLayout_, &lumaNumRows_, &lumaRowSizeInBytes, &lumaRequiredSize);

        D3D12_RESOURCE_DESC chromaDesc = slot.chromaTexture->GetResource()->GetDesc();
        UINT64 chromaRequiredSize = 0;
        UINT64 chromaRowSizeInBytes = 0;
        device->GetCopyableFootprints(&chromaDesc, 0, 1, 0, &chromaLayout_, &chromaNumRows_, &chromaRowSizeInBytes, &chromaRequiredSize);
    }

    if (!CreatePersistentUploadBuffer(device, lumaLayout_.Footprint.RowPitch * lumaNumRows_, slot.lumaUpload, slot.lumaMapped)) {
        return false;
    }
    if (!CreatePersistentUploadBuffer(device, chromaLayout_.Footprint.RowPitch * chromaNumRows_, slot.chromaUpload, slot.chromaMapped)) {
        return false;
    }

    return true;
}

bool VideoTexture::InitializeRgbaOutput() {
    LogScope scope;
    rgbaUav_ = std::make_unique<UnorderedAccessResource>(width_, height_, DXGI_FORMAT_R8G8B8A8_UNORM);
    if (!rgbaUav_->GetResource()) return false;
    // UnorderedAccessResourceはUNORDERED_ACCESS状態のみを登録して生成されるため、
    // 描画側で読む際のPIXEL_SHADER_RESOURCEへの遷移もTransitionToで正しく追跡できるよう
    // RWStructuredBufferResource(createSrv=true時)と同様に2状態目を登録しておく
    rgbaUav_->AddTransitionState(D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE);

    // 変換結果を通常のテクスチャとして読むためのSRVを同一リソースに対して追加で作成する。
    // 状態遷移バリアはrgbaUav_側のみが発行する（このSRVインスタンスの状態追跡は使わない）
    // GetRgbaView()（マテリアルのテクスチャとして参照される経路）が返すのはこのSRVのため、
    // バインドレステーブル用の予約レンジから確保する
    rgbaSrv_ = std::make_unique<ShaderResourceResource>(
        width_, height_, DXGI_FORMAT_R8G8B8A8_UNORM, D3D12_RESOURCE_FLAG_ALLOW_UNORDERED_ACCESS,
        rgbaUav_->GetResource(), D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE, 1, 1, nullptr, /*useReservedRange=*/true);
    if (!rgbaSrv_->GetResource()) return false;

    return true;
}

bool VideoTexture::UploadFrame(const std::uint8_t *nv12Data, std::size_t dataSize, std::uint32_t sourceStride) {
    LogScope scope;
    if (!valid_ || !nv12Data) return false;

    const std::uint32_t stride = (sourceStride != 0) ? sourceStride : width_;

    // NV12バッファは本来 stride×height×1.5 バイトになるが、H.264等のデコーダーはマクロブロック境界
    // (16の倍数)に合わせて内部バッファの高さを表示上の高さ(height_)より切り上げて確保することが多い
    // （例: 表示1080pでも実バッファは1088行）。決め打ちで height_ を使うとUV(色差)平面の開始位置が
    // ずれて色が壊れるため、実際に渡されたバイト数から実バッファの行数を逆算する
    std::uint32_t sourceHeight = height_;
    if (stride != 0) {
        const std::size_t totalRowUnits = static_cast<std::size_t>(stride) * 3; // stride*1.5 の2倍（整数演算のため）
        if (totalRowUnits != 0) {
            const std::uint32_t computedHeight = static_cast<std::uint32_t>((dataSize * 2) / totalRowUnits);
            if (computedHeight >= height_) sourceHeight = computedHeight;
        }
    }

    const std::size_t lumaBytes = static_cast<std::size_t>(stride) * sourceHeight;
    const std::size_t chromaBytes = static_cast<std::size_t>(stride) * (sourceHeight / 2);
    if (dataSize < lumaBytes + chromaBytes) {
        Log(Translation("engine.video.texture.upload.failed.size"), LogSeverity::Warning);
        return false;
    }

    YuvSlot &writeSlot = yuvSlots_[writeIndex_];

    // このスロットへ前回発行したGPUコピーが完了しているかを確認する。未完了ならUpload heapを
    // 上書きするとGPU側がまだ読み取り中のデータを壊してしまうため、今回は何もせず呼び出し元に
    // 次のUpdateでの再試行を委ねる
    if (writeSlot.uploadFenceValue != 0 &&
        !directXCommon_->IsVideoUploadFenceComplete(Passkey<VideoTexture>{}, writeSlot.uploadFenceValue)) {
        return false;
    }

    const std::uint8_t *lumaSrc = nv12Data;
    const std::uint8_t *chromaSrc = nv12Data + lumaBytes;

    {
        auto *dst = static_cast<std::uint8_t *>(writeSlot.lumaMapped);
        for (UINT y = 0; y < lumaNumRows_; ++y) {
            std::memcpy(dst + static_cast<std::size_t>(y) * lumaLayout_.Footprint.RowPitch,
                lumaSrc + static_cast<std::size_t>(y) * stride, width_);
        }
    }
    {
        auto *dst = static_cast<std::uint8_t *>(writeSlot.chromaMapped);
        const std::size_t chromaRowBytes = static_cast<std::size_t>(chromaWidth_) * 2;
        for (UINT y = 0; y < chromaNumRows_; ++y) {
            std::memcpy(dst + static_cast<std::size_t>(y) * chromaLayout_.Footprint.RowPitch,
                chromaSrc + static_cast<std::size_t>(y) * stride, chromaRowBytes);
        }
    }

    ShaderResourceResource *lumaTexture = writeSlot.lumaTexture.get();
    ShaderResourceResource *chromaTexture = writeSlot.chromaTexture.get();
    ID3D12Resource *lumaUpload = writeSlot.lumaUpload.Get();
    ID3D12Resource *chromaUpload = writeSlot.chromaUpload.Get();
    const D3D12_PLACED_SUBRESOURCE_FOOTPRINT &lumaLayout = lumaLayout_;
    const D3D12_PLACED_SUBRESOURCE_FOOTPRINT &chromaLayout = chromaLayout_;

    const std::uint64_t fenceValue = directXCommon_->ExecuteOneShotCommandsForVideoTexture(
        Passkey<VideoTexture>{}, [&](ID3D12GraphicsCommandList *cl) {
            lumaTexture->SetCommandList(cl);
            lumaTexture->TransitionTo(D3D12_RESOURCE_STATE_COPY_DEST);
            chromaTexture->SetCommandList(cl);
            chromaTexture->TransitionTo(D3D12_RESOURCE_STATE_COPY_DEST);

            D3D12_TEXTURE_COPY_LOCATION lumaDst{};
            lumaDst.pResource = lumaTexture->GetResource();
            lumaDst.Type = D3D12_TEXTURE_COPY_TYPE_SUBRESOURCE_INDEX;
            lumaDst.SubresourceIndex = 0;
            D3D12_TEXTURE_COPY_LOCATION lumaSrcLoc{};
            lumaSrcLoc.pResource = lumaUpload;
            lumaSrcLoc.Type = D3D12_TEXTURE_COPY_TYPE_PLACED_FOOTPRINT;
            lumaSrcLoc.PlacedFootprint = lumaLayout;
            cl->CopyTextureRegion(&lumaDst, 0, 0, 0, &lumaSrcLoc, nullptr);

            D3D12_TEXTURE_COPY_LOCATION chromaDst{};
            chromaDst.pResource = chromaTexture->GetResource();
            chromaDst.Type = D3D12_TEXTURE_COPY_TYPE_SUBRESOURCE_INDEX;
            chromaDst.SubresourceIndex = 0;
            D3D12_TEXTURE_COPY_LOCATION chromaSrcLoc{};
            chromaSrcLoc.pResource = chromaUpload;
            chromaSrcLoc.Type = D3D12_TEXTURE_COPY_TYPE_PLACED_FOOTPRINT;
            chromaSrcLoc.PlacedFootprint = chromaLayout;
            cl->CopyTextureRegion(&chromaDst, 0, 0, 0, &chromaSrcLoc, nullptr);

            // 以降はコンピュートシェーダー(Renderer::ProcessVideoConversions)だけが読むため
            // NON_PIXEL_SHADER_RESOURCEへ遷移する
            lumaTexture->TransitionTo(D3D12_RESOURCE_STATE_NON_PIXEL_SHADER_RESOURCE);
            chromaTexture->TransitionTo(D3D12_RESOURCE_STATE_NON_PIXEL_SHADER_RESOURCE);
        });

    if (fenceValue == 0) return false;
    writeSlot.uploadFenceValue = fenceValue;

    // 共有DIRECTキューへの投入順序がそのままGPU実行順序になるため（FIFO）、この後同じフレーム内で
    // 積まれるコンピュート変換パス(Renderer::ProcessVideoConversions)は必ずこのコピーの後に実行される。
    // そのため読み取りインデックスの切り替えはGPU完了を待たずに即座に行ってよい
    readIndex_ = writeIndex_;
    writeIndex_ = (writeIndex_ + 1) % kBufferCount;
    pendingConversion_ = true;
    return true;
}

IShaderTexture *VideoTexture::GetRgbaView() const noexcept { LogScope scope; return rgbaView_.get(); }

ShaderResourceResource *VideoTexture::GetLumaResource(Passkey<Renderer>) const noexcept {
    LogScope scope;
    return yuvSlots_[readIndex_].lumaTexture.get();
}
ShaderResourceResource *VideoTexture::GetChromaResource(Passkey<Renderer>) const noexcept {
    LogScope scope;
    return yuvSlots_[readIndex_].chromaTexture.get();
}
UnorderedAccessResource *VideoTexture::GetRgbaUavResource(Passkey<Renderer>) const noexcept {
    LogScope scope;
    return rgbaUav_.get();
}

D3D12_GPU_DESCRIPTOR_HANDLE VideoTexture::GetRgbaSrvHandle() const noexcept {
    LogScope scope;
    return rgbaSrv_ ? rgbaSrv_->GetGPUDescriptorHandle() : D3D12_GPU_DESCRIPTOR_HANDLE{};
}

} // namespace KashipanEngine
