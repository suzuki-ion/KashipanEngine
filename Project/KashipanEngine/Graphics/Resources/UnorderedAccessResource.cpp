#include "UnorderedAccessResource.h"

namespace KashipanEngine {

UnorderedAccessResource::UnorderedAccessResource(UINT width, UINT height, DXGI_FORMAT format, ID3D12Resource *existingResource)
    : IGraphicsResource(ResourceViewType::UAV) {
    Initialize(width, height, format, existingResource);
}

bool UnorderedAccessResource::Recreate(UINT width, UINT height, DXGI_FORMAT format, ID3D12Resource *existingResource) {
    ResetResourceForRecreate();
    return Initialize(width, height, format, existingResource);
}

bool UnorderedAccessResource::Initialize(UINT width, UINT height, DXGI_FORMAT format, ID3D12Resource *existingResource) {
    LogScope scope;
    auto *uavHeap = GetSRVHeap();
    if (!GetDevice() || !uavHeap) {
        Log(Translation("engine.graphics.resource.create.device.null"), LogSeverity::Warning);
        return false;
    }

    width_ = width;
    height_ = height;
    format_ = format;

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
    resourceDesc.Flags = D3D12_RESOURCE_FLAG_ALLOW_UNORDERED_ACCESS;

    D3D12_HEAP_PROPERTIES heapProps = {};
    heapProps.Type = D3D12_HEAP_TYPE_DEFAULT;

    ClearTransitionStates();
    AddTransitionState(D3D12_RESOURCE_STATE_UNORDERED_ACCESS);

    if (existingResource) {
        SetExistingResource(existingResource);
    } else {
        CreateResource(L"Unordered Access Resource", &heapProps, D3D12_HEAP_FLAG_NONE, &resourceDesc, nullptr);
        if (!GetResource()) {
            return false;
        }
    }

    auto handle = uavHeap->AllocateDescriptorHandle();

    D3D12_UNORDERED_ACCESS_VIEW_DESC uavDesc = {};
    uavDesc.Format = format_;
    uavDesc.ViewDimension = D3D12_UAV_DIMENSION_TEXTURE2D;

    GetDevice()->CreateUnorderedAccessView(GetResource(), nullptr, &uavDesc, handle->cpuHandle);

    SetDescriptorHandleInfo(std::move(handle));
    return true;
}

} // namespace KashipanEngine
