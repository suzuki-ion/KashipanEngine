#include "DepthStencilResource.h"

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
    DXGI_FORMAT srvFormat)
    : IGraphicsResource(ResourceViewType::DSV) {
    Initialize(width, height, format, clearDepth, clearStencil, existingResource, createSrv, srvFormat);
}

bool DepthStencilResource::Recreate(UINT width, UINT height, DXGI_FORMAT format,
    FLOAT clearDepth, UINT8 clearStencil,
    ID3D12Resource *existingResource,
    bool createSrv,
    DXGI_FORMAT srvFormat) {
    ResetResourceForRecreate();
    srvHandleInfo_.reset();
    return Initialize(width, height, format, clearDepth, clearStencil, existingResource, createSrv, srvFormat);
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

void DepthStencilResource::CreateSrvInternal(DXGI_FORMAT srvFormat) {
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

    srvHandleInfo_ = srvHeap->AllocateDescriptorHandle();

    D3D12_SHADER_RESOURCE_VIEW_DESC srvDesc = {};
    srvDesc.Format = srvFormat;
    srvDesc.ViewDimension = D3D12_SRV_DIMENSION_TEXTURE2D;
    srvDesc.Shader4ComponentMapping = D3D12_DEFAULT_SHADER_4_COMPONENT_MAPPING;
    srvDesc.Texture2D.MipLevels = 1;

    GetDevice()->CreateShaderResourceView(GetResource(), &srvDesc, srvHandleInfo_->cpuHandle);
}

bool DepthStencilResource::Initialize(UINT width, UINT height, DXGI_FORMAT format, FLOAT clearDepth, UINT8 clearStencil,
    ID3D12Resource *existingResource,
    bool createSrv,
    DXGI_FORMAT srvFormat) {
    LogScope scope;
    auto *dsvHeap = GetDSVHeap();
    if (!GetDevice() || !dsvHeap) {
        Log(Translation("engine.graphics.resource.create.device.null"), LogSeverity::Warning);
        return false;
    }

    width_ = width;
    height_ = height;
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
    resourceDesc.DepthOrArraySize = 1;
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
    dsvDesc.ViewDimension = D3D12_DSV_DIMENSION_TEXTURE2D;
    dsvDesc.Flags = D3D12_DSV_FLAG_NONE;

    GetDevice()->CreateDepthStencilView(GetResource(), &dsvDesc, handle->cpuHandle);

    SetDescriptorHandleInfo(std::move(handle));

    if (createSrv) {
        CreateSrvInternal(srvFormat);
    }

    return true;
}

} // namespace KashipanEngine
