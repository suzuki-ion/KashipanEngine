#include "RenderTargetResource.h"

namespace KashipanEngine {

RenderTargetResource::RenderTargetResource(UINT width, UINT height, DXGI_FORMAT format, RTVHeap *rtvHeap, ID3D12Resource *existingResource, const FLOAT clearColor[4])
    : IGraphicsResource(ResourceViewType::RTV) {
    rtvHeap_ = rtvHeap;
    Initialize(width, height, format, existingResource, clearColor);
}

bool RenderTargetResource::Recreate(UINT width, UINT height, DXGI_FORMAT format, ID3D12Resource *existingResource, const FLOAT clearColor[4]) {
    ResetResourceForRecreate();
    return Initialize(width, height, format, existingResource, clearColor);
}

void RenderTargetResource::ClearRenderTargetView() const {
    auto *cl = GetCommandList();
    if (!cl || !GetDescriptorHandleInfo()) {
        return;
    }
    cl->ClearRenderTargetView(
        GetDescriptorHandleInfo()->cpuHandle,
        clearColor_,
        0,
        nullptr
    );
}

bool RenderTargetResource::Initialize(UINT width, UINT height, DXGI_FORMAT format, ID3D12Resource *existingResource, const FLOAT clearColor[4]) {
    LogScope scope;
    if (!GetDevice() || !rtvHeap_) {
        Log(Translation("engine.graphics.resource.create.device.null"), LogSeverity::Warning);
        return false;
    }

    width_ = width;
    height_ = height;
    format_ = format;
    if (clearColor) {
        memcpy(clearColor_, clearColor, sizeof(clearColor_));
    }

    // クリア値
    D3D12_CLEAR_VALUE clearValue = {};
    clearValue.Format = format_;
    clearValue.Color[0] = clearColor_[0];
    clearValue.Color[1] = clearColor_[1];
    clearValue.Color[2] = clearColor_[2];
    clearValue.Color[3] = clearColor_[3];

    // リソース記述
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
    resourceDesc.Flags = D3D12_RESOURCE_FLAG_ALLOW_RENDER_TARGET;

    // ヒーププロパティ
    D3D12_HEAP_PROPERTIES heapProps = {};
    heapProps.Type = D3D12_HEAP_TYPE_DEFAULT;

    // 状態遷移設定
    ClearTransitionStates();
    AddTransitionState(D3D12_RESOURCE_STATE_RENDER_TARGET);

    // 既存リソース設定
    if (existingResource) {
        SetExistingResource(existingResource);
    } else {
        CreateResource(L"Render Target Resource", &heapProps, D3D12_HEAP_FLAG_NONE, &resourceDesc, &clearValue);
        if (!GetResource()) {
            return false;
        }
    }

    // RTV作成
    auto handle = rtvHeap_->AllocateDescriptorHandle();

    D3D12_RENDER_TARGET_VIEW_DESC rtvDesc = {};
    rtvDesc.Format = format_;
    rtvDesc.ViewDimension = D3D12_RTV_DIMENSION_TEXTURE2D;
    rtvDesc.Texture2D.MipSlice = 0;

    GetDevice()->CreateRenderTargetView(GetResource(), &rtvDesc, handle->cpuHandle);

    SetDescriptorHandleInfo(std::move(handle));
    return true;
}

} // namespace KashipanEngine
