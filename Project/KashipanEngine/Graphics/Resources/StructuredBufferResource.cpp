#include "StructuredBufferResource.h"

namespace KashipanEngine {

StructuredBufferResource::StructuredBufferResource(size_t elementStride, size_t elementCount, const void* initialData, ID3D12Resource* existingResource)
    : IGraphicsResource(ResourceViewType::SRV) {
    Initialize(elementStride, elementCount, initialData, existingResource);
}

bool StructuredBufferResource::Recreate(size_t elementStride, size_t elementCount, const void* initialData, ID3D12Resource* existingResource) {
    ResetMappedPointer_();
    ResetResourceForRecreate();
    return Initialize(elementStride, elementCount, initialData, existingResource);
}

bool StructuredBufferResource::Initialize(size_t elementStride, size_t elementCount, const void* initialData, ID3D12Resource* existingResource) {
    LogScope scope;
    auto* srvHeap = GetSRVHeap();
    if (!GetDevice() || !srvHeap) {
        Log(Translation("engine.graphics.resource.create.device.null"), LogSeverity::Warning);
        return false;
    }

    elementStride_ = elementStride;
    elementCount_ = elementCount;
    bufferSize_ = elementStride_ * elementCount_;

    if (elementStride_ == 0 || elementCount_ == 0 || bufferSize_ == 0) {
        return false;
    }

    D3D12_HEAP_PROPERTIES heapProps{};
    heapProps.Type = D3D12_HEAP_TYPE_UPLOAD;

    D3D12_RESOURCE_DESC bufDesc{};
    bufDesc.Dimension = D3D12_RESOURCE_DIMENSION_BUFFER;
    bufDesc.Width = bufferSize_;
    bufDesc.Height = 1;
    bufDesc.DepthOrArraySize = 1;
    bufDesc.MipLevels = 1;
    bufDesc.Layout = D3D12_TEXTURE_LAYOUT_ROW_MAJOR;
    bufDesc.SampleDesc.Count = 1;

    ClearTransitionStates();
    AddTransitionState(D3D12_RESOURCE_STATE_GENERIC_READ);

    if (existingResource) {
        SetExistingResource(existingResource);
    } else {
        CreateResource(L"Structured Buffer Resource", &heapProps, D3D12_HEAP_FLAG_NONE, &bufDesc, nullptr);
        if (!GetResource()) return false;
    }

    if (initialData) {
        void* dst = Map();
        if (!dst) return false;
        memcpy(dst, initialData, bufferSize_);
    }

    auto handle = srvHeap->AllocateDescriptorHandle();

    D3D12_SHADER_RESOURCE_VIEW_DESC srvDesc{};
    srvDesc.Format = DXGI_FORMAT_UNKNOWN;
    srvDesc.ViewDimension = D3D12_SRV_DIMENSION_BUFFER;
    srvDesc.Shader4ComponentMapping = D3D12_DEFAULT_SHADER_4_COMPONENT_MAPPING;
    srvDesc.Buffer.FirstElement = 0;
    srvDesc.Buffer.NumElements = static_cast<UINT>(elementCount_);
    srvDesc.Buffer.StructureByteStride = static_cast<UINT>(elementStride_);
    srvDesc.Buffer.Flags = D3D12_BUFFER_SRV_FLAG_NONE;

    GetDevice()->CreateShaderResourceView(GetResource(), &srvDesc, handle->cpuHandle);
    SetDescriptorHandleInfo(std::move(handle));

    return true;
}

void* StructuredBufferResource::Map() {
    if (mappedPtr_) return mappedPtr_;

    void* ptr = nullptr;
    if (GetResource()) {
        GetResource()->Map(0, nullptr, &ptr);
    }
    mappedPtr_ = ptr;
    return mappedPtr_;
}

void StructuredBufferResource::Unmap() {
    // Upload heap は永続Mapしておく想定。互換のためAPIは残す。
}

} // namespace KashipanEngine
