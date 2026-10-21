#include "DepthStencilResource.h"
#include "Utilities/Translation.h"

namespace KashipanEngine {

namespace {
bool IsDepthStencilFormat(DXGI_FORMAT f) noexcept {
    switch (f) {
    case DXGI_FORMAT_D16_UNORM:
    case DXGI_FORMAT_D24_UNORM_S8_UINT:
    case DXGI_FORMAT_D32_FLOAT:
    case DXGI_FORMAT_D32_FLOAT_S8X24_UINT:
        return true;
    default:
        return false;
    }
}

DXGI_FORMAT ToTypelessForDepth(DXGI_FORMAT f) noexcept {
    switch (f) {
    case DXGI_FORMAT_D16_UNORM:
        return DXGI_FORMAT_R16_TYPELESS;
    case DXGI_FORMAT_D24_UNORM_S8_UINT:
        return DXGI_FORMAT_R24G8_TYPELESS;
    case DXGI_FORMAT_D32_FLOAT:
        return DXGI_FORMAT_R32_TYPELESS;
    case DXGI_FORMAT_D32_FLOAT_S8X24_UINT:
        return DXGI_FORMAT_R32G8X24_TYPELESS;
    default:
        return f;
    }
}
}

DepthStencilResource::DepthStencilResource(UINT width, UINT height, DXGI_FORMAT format,
    FLOAT clearDepth, UINT8 clearStencil,
    ID3D12Resource *existingResource,
    bool createSrv,
    DXGI_FORMAT srvFormat,
    UINT arraySize,
    bool srvUseReservedRange)
    : IGraphicsResource(ResourceViewType::DSV) {
    Initialize(width, height, format, clearDepth, clearStencil, existingResource, createSrv, srvFormat, arraySize, srvUseReservedRange);
}

bool DepthStencilResource::Recreate(UINT width, UINT height, DXGI_FORMAT format,
    FLOAT clearDepth, UINT8 clearStencil,
    ID3D12Resource *existingResource,
    bool createSrv,
    DXGI_FORMAT srvFormat,
    UINT arraySize,
    bool srvUseReservedRange) {
    ResetResourceForRecreate();
    srvHandleInfo_.reset();
    sliceDsvHandles_.clear();
    return Initialize(width, height, format, clearDepth, clearStencil, existingResource, createSrv, srvFormat, arraySize, srvUseReservedRange);
}

D3D12_CPU_DESCRIPTOR_HANDLE DepthStencilResource::GetSliceDsvHandle(UINT slice) const {
    if (arraySize_ >= 2 && slice < sliceDsvHandles_.size() && sliceDsvHandles_[slice]) {
        return sliceDsvHandles_[slice]->cpuHandle;
    }
    // 配列でない場合（またはスライス不正時）は通常のDSVを返す
    const auto *handleInfo = GetDescriptorHandleInfo();
    return handleInfo ? handleInfo->cpuHandle : D3D12_CPU_DESCRIPTOR_HANDLE{};
}

void DepthStencilResource::ClearDepthStencilView() const {
    auto *cl = GetCommandList();
    if (!cl || !GetDescriptorHandleInfo()) {
        return;
    }
    cl->ClearDepthStencilView(
        GetDescriptorHandleInfo()->cpuHandle,
        D3D12_CLEAR_FLAG_DEPTH | D3D12_CLEAR_FLAG_STENCIL,
        clearDepth_,
        clearStencil_,
        0,
        nullptr
    );
}

DXGI_FORMAT DepthStencilResource::GuessSrvFormatFromDsvFormat(DXGI_FORMAT dsvFormat) noexcept {
    switch (dsvFormat) {
    case DXGI_FORMAT_D16_UNORM:
        return DXGI_FORMAT_R16_UNORM;
    case DXGI_FORMAT_D24_UNORM_S8_UINT:
        return DXGI_FORMAT_R24_UNORM_X8_TYPELESS;
    case DXGI_FORMAT_D32_FLOAT:
        return DXGI_FORMAT_R32_FLOAT;
    case DXGI_FORMAT_D32_FLOAT_S8X24_UINT:
        return DXGI_FORMAT_R32_FLOAT_X8X24_TYPELESS;
    default:
        return DXGI_FORMAT_UNKNOWN;
    }
}

void DepthStencilResource::CreateSrvInternal(DXGI_FORMAT srvFormat, bool useReservedRange) {
    auto *srvHeap = GetSRVHeap();
    if (!GetDevice() || !srvHeap || !GetResource()) {
        return;
    }

    if (srvFormat == DXGI_FORMAT_UNKNOWN) {
        srvFormat = GuessSrvFormatFromDsvFormat(format_);
    }
    if (srvFormat == DXGI_FORMAT_UNKNOWN) {
        return;
    }

    srvHandleInfo_ = useReservedRange ? srvHeap->AllocateReservedDescriptorHandle() : srvHeap->AllocateDescriptorHandle();

    D3D12_SHADER_RESOURCE_VIEW_DESC srvDesc = {};
    srvDesc.Format = srvFormat;
    srvDesc.Shader4ComponentMapping = D3D12_DEFAULT_SHADER_4_COMPONENT_MAPPING;
    if (arraySize_ >= 2) {
        srvDesc.ViewDimension = D3D12_SRV_DIMENSION_TEXTURE2DARRAY;
        srvDesc.Texture2DArray.MipLevels = 1;
        srvDesc.Texture2DArray.FirstArraySlice = 0;
        srvDesc.Texture2DArray.ArraySize = arraySize_;
    } else {
        srvDesc.ViewDimension = D3D12_SRV_DIMENSION_TEXTURE2D;
        srvDesc.Texture2D.MipLevels = 1;
    }

    GetDevice()->CreateShaderResourceView(GetResource(), &srvDesc, srvHandleInfo_->cpuHandle);
}

bool DepthStencilResource::Initialize(UINT width, UINT height, DXGI_FORMAT format, FLOAT clearDepth, UINT8 clearStencil,
    ID3D12Resource *existingResource,
    bool createSrv,
    DXGI_FORMAT srvFormat,
    UINT arraySize,
    bool srvUseReservedRange) {
    LogScope scope;
    auto *dsvHeap = GetDSVHeap();
    if (!GetDevice() || !dsvHeap) {
        Log(Translation("engine.graphics.resource.create.device.null"), LogSeverity::Warning);
        return false;
    }

    width_ = width;
    height_ = height;
    arraySize_ = (arraySize == 0) ? 1 : arraySize;
    format_ = format;
    clearDepth_ = clearDepth;
    clearStencil_ = clearStencil;

    D3D12_CLEAR_VALUE clearValue = {};
    clearValue.Format = format_;
    clearValue.DepthStencil.Depth = clearDepth_;
    clearValue.DepthStencil.Stencil = clearStencil_;

    const bool needsTypeless = createSrv && !existingResource && IsDepthStencilFormat(format_);

    D3D12_RESOURCE_DESC resourceDesc = {};
    resourceDesc.Dimension = D3D12_RESOURCE_DIMENSION_TEXTURE2D;
    resourceDesc.Alignment = 0;
    resourceDesc.Width = width_;
    resourceDesc.Height = height_;
    resourceDesc.DepthOrArraySize = static_cast<UINT16>(arraySize_);
    resourceDesc.MipLevels = 1;
    resourceDesc.Format = needsTypeless ? ToTypelessForDepth(format_) : format_;
    resourceDesc.SampleDesc = {1, 0};
    resourceDesc.Layout = D3D12_TEXTURE_LAYOUT_UNKNOWN;
    resourceDesc.Flags = D3D12_RESOURCE_FLAG_ALLOW_DEPTH_STENCIL;

    D3D12_HEAP_PROPERTIES heapProps = {};
    heapProps.Type = D3D12_HEAP_TYPE_DEFAULT;

    ClearTransitionStates();
    AddTransitionState(D3D12_RESOURCE_STATE_DEPTH_WRITE);
    AddTransitionState(D3D12_RESOURCE_STATE_DEPTH_READ);
    if (createSrv)
        AddTransitionState(D3D12_RESOURCE_STATE_NON_PIXEL_SHADER_RESOURCE | D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE);

    if (existingResource) {
        SetExistingResource(existingResource);
    } else {
        CreateResource(L"Depth Stencil Resource", &heapProps, D3D12_HEAP_FLAG_NONE, &resourceDesc, &clearValue);
        if (!GetResource()) {
            return false;
        }
    }

    auto handle = dsvHeap->AllocateDescriptorHandle();

    D3D12_DEPTH_STENCIL_VIEW_DESC dsvDesc = {};
    dsvDesc.Format = format_;
    dsvDesc.Flags = D3D12_DSV_FLAG_NONE;
    if (arraySize_ >= 2) {
        // 全スライスを対象としたDSV（ClearDepthStencilViewで一括クリアするために使用）
        dsvDesc.ViewDimension = D3D12_DSV_DIMENSION_TEXTURE2DARRAY;
        dsvDesc.Texture2DArray.MipSlice = 0;
        dsvDesc.Texture2DArray.FirstArraySlice = 0;
        dsvDesc.Texture2DArray.ArraySize = arraySize_;
    } else {
        dsvDesc.ViewDimension = D3D12_DSV_DIMENSION_TEXTURE2D;
    }

    GetDevice()->CreateDepthStencilView(GetResource(), &dsvDesc, handle->cpuHandle);

    SetDescriptorHandleInfo(std::move(handle));

    // 配列の場合はスライスごとの描画先用DSVも作成する
    if (arraySize_ >= 2) {
        sliceDsvHandles_.reserve(arraySize_);
        for (UINT i = 0; i < arraySize_; ++i) {
            auto sliceHandle = dsvHeap->AllocateDescriptorHandle();
            D3D12_DEPTH_STENCIL_VIEW_DESC sliceDesc = {};
            sliceDesc.Format = format_;
            sliceDesc.Flags = D3D12_DSV_FLAG_NONE;
            sliceDesc.ViewDimension = D3D12_DSV_DIMENSION_TEXTURE2DARRAY;
            sliceDesc.Texture2DArray.MipSlice = 0;
            sliceDesc.Texture2DArray.FirstArraySlice = i;
            sliceDesc.Texture2DArray.ArraySize = 1;
            GetDevice()->CreateDepthStencilView(GetResource(), &sliceDesc, sliceHandle->cpuHandle);
            sliceDsvHandles_.push_back(std::move(sliceHandle));
        }
    }

    if (createSrv) {
        CreateSrvInternal(srvFormat, srvUseReservedRange);
    }

    return true;
}

} // namespace KashipanEngine
