#include "ShaderResourceResource.h"

namespace KashipanEngine {

ShaderResourceResource::ShaderResourceResource(UINT width, UINT height, DXGI_FORMAT format, D3D12_RESOURCE_FLAGS flags, ID3D12Resource *existingResource)
    : IGraphicsResource(ResourceViewType::SRV) {
    Initialize(width, height, format, flags, existingResource);
}

bool ShaderResourceResource::Recreate(UINT width, UINT height, DXGI_FORMAT format, D3D12_RESOURCE_FLAGS flags, ID3D12Resource *existingResource) {
    ResetResourceForRecreate();
    return Initialize(width, height, format, flags, existingResource);
}

bool ShaderResourceResource::Initialize(UINT width, UINT height, DXGI_FORMAT format, D3D12_RESOURCE_FLAGS flags, ID3D12Resource *existingResource) {
    LogScope scope;
    auto *srvHeap = GetSRVHeap();
    if (!GetDevice() || !srvHeap) {
        Log(Translation("engine.graphics.resource.create.device.null"), LogSeverity::Warning);
        return false;
    }

    width_ = width;
    height_ = height;
    format_ = format;
    flags_ = flags;

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
    resourceDesc.Flags = flags_;

    D3D12_HEAP_PROPERTIES heapProps = {};
    heapProps.Type = D3D12_HEAP_TYPE_DEFAULT;

    ClearTransitionStates();
    AddTransitionState(D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE);

    if (existingResource) {
        SetExistingResource(existingResource);
    } else {
        CreateResource(L"Shader Resource Resource", &heapProps, D3D12_HEAP_FLAG_NONE, &resourceDesc, nullptr);
        if (!GetResource()) {
            return false;
        }
    }

    auto handle = srvHeap->AllocateDescriptorHandle();

    D3D12_SHADER_RESOURCE_VIEW_DESC srvDesc = {};
    srvDesc.Format = format_;
    srvDesc.ViewDimension = D3D12_SRV_DIMENSION_TEXTURE2D;
    srvDesc.Shader4ComponentMapping = D3D12_DEFAULT_SHADER_4_COMPONENT_MAPPING;
    srvDesc.Texture2D.MipLevels = 1;

    GetDevice()->CreateShaderResourceView(GetResource(), &srvDesc, handle->cpuHandle);

    SetDescriptorHandleInfo(std::move(handle));
    return true;
}

} // namespace KashipanEngine
