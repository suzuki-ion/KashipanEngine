#include "DepthStencilResource.h"

namespace KashipanEngine {

DepthStencilResource::DepthStencilResource(UINT width, UINT height, DXGI_FORMAT format, FLOAT clearDepth, UINT8 clearStencil, DSVHeap *dsvHeap, ID3D12Resource *existingResource)
    : IGraphicsResource(ResourceViewType::DSV) {
    dsvHeap_ = dsvHeap;
    Initialize(width, height, format, clearDepth, clearStencil, existingResource);
}

bool DepthStencilResource::Recreate(UINT width, UINT height, DXGI_FORMAT format, FLOAT clearDepth, UINT8 clearStencil, ID3D12Resource *existingResource) {
    ResetResourceForRecreate();
    return Initialize(width, height, format, clearDepth, clearStencil, existingResource);
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

bool DepthStencilResource::Initialize(UINT width, UINT height, DXGI_FORMAT format, FLOAT clearDepth, UINT8 clearStencil, ID3D12Resource *existingResource) {
    LogScope scope;
    if (!GetDevice() || !dsvHeap_) {
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

    D3D12_RESOURCE_DESC resourceDesc = {};
    resourceDesc.Dimension = D3D12_RESOURCE_DIMENSION_TEXTURE2D;
    resourceDesc.Alignment = 0;
    resourceDesc.Width = width_;
    resourceDesc.Height = height_;
    resourceDesc.DepthOrArraySize = 1;
    resourceDesc.MipLevels = 1;
    resourceDesc.Format = format_;
    resourceDesc.SampleDesc = {1, 0};
    resourceDesc.Layout = D3D12_TEXTURE_LAYOUT_UNKNOWN;
    resourceDesc.Flags = D3D12_RESOURCE_FLAG_ALLOW_DEPTH_STENCIL;

    D3D12_HEAP_PROPERTIES heapProps = {};
    heapProps.Type = D3D12_HEAP_TYPE_DEFAULT;

    ClearTransitionStates();
    AddTransitionState(D3D12_RESOURCE_STATE_DEPTH_WRITE);

    if (existingResource) {
        SetExistingResource(existingResource);
    } else {
        CreateResource(L"Depth Stencil Resource", &heapProps, D3D12_HEAP_FLAG_NONE, &resourceDesc, &clearValue);
        if (!GetResource()) {
            return false;
        }
    }

    auto handle = dsvHeap_->AllocateDescriptorHandle();

    D3D12_DEPTH_STENCIL_VIEW_DESC dsvDesc = {};
    dsvDesc.Format = format_;
    dsvDesc.ViewDimension = D3D12_DSV_DIMENSION_TEXTURE2D;
    dsvDesc.Flags = D3D12_DSV_FLAG_NONE;

    GetDevice()->CreateDepthStencilView(GetResource(), &dsvDesc, handle->cpuHandle);

    SetDescriptorHandleInfo(std::move(handle));
    return true;
}

} // namespace KashipanEngine
