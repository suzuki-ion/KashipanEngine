#include "ConstantBufferResource.h"

namespace KashipanEngine {

static inline size_t Align256(size_t size) { return (size + 255) & ~static_cast<size_t>(255); }

ConstantBufferResource::ConstantBufferResource(size_t byteSize, CBVHeap *cbvHeap, ID3D12Resource *existingResource)
    : IGraphicsResource(ResourceViewType::CBV) {
    cbvHeap_ = cbvHeap;
    Initialize(byteSize, existingResource);
}

bool ConstantBufferResource::Recreate(size_t byteSize, ID3D12Resource *existingResource) {
    ResetResourceForRecreate();
    return Initialize(byteSize, existingResource);
}

bool ConstantBufferResource::Initialize(size_t byteSize, ID3D12Resource *existingResource) {
    LogScope scope;
    if (!GetDevice() || !cbvHeap_) {
        Log(Translation("engine.graphics.resource.create.device.null"), LogSeverity::Warning);
        return false;
    }

    size_t alignedSize = Align256(byteSize);

    D3D12_HEAP_PROPERTIES heapProps = {};
    heapProps.Type = D3D12_HEAP_TYPE_UPLOAD;

    D3D12_RESOURCE_DESC bufDesc = {};
    bufDesc.Dimension = D3D12_RESOURCE_DIMENSION_BUFFER;
    bufDesc.Width = alignedSize;
    bufDesc.Height = 1;
    bufDesc.DepthOrArraySize = 1;
    bufDesc.MipLevels = 1;
    bufDesc.Layout = D3D12_TEXTURE_LAYOUT_ROW_MAJOR;
    bufDesc.SampleDesc.Count = 1;

    ClearTransitionStates();
    AddTransitionState(D3D12_RESOURCE_STATE_GENERIC_READ);

    if (existingResource) {
        SetExistingResource(existingResource);
        // Derive size from provided resource if possible
        bufferSize_ = alignedSize; // fallback
        D3D12_RESOURCE_DESC desc = existingResource->GetDesc();
        if (desc.Dimension == D3D12_RESOURCE_DIMENSION_BUFFER) {
            bufferSize_ = static_cast<size_t>(desc.Width);
        }
    } else {
        bufferSize_ = alignedSize;
        CreateResource(L"Constant Buffer Resource", &heapProps, D3D12_HEAP_FLAG_NONE, &bufDesc, nullptr);
        if (!GetResource()) {
            return false;
        }
    }

    auto handle = cbvHeap_->AllocateDescriptorHandle();

    D3D12_CONSTANT_BUFFER_VIEW_DESC cbvDesc = {};
    cbvDesc.BufferLocation = GetResource()->GetGPUVirtualAddress();
    cbvDesc.SizeInBytes = static_cast<UINT>(Align256(bufferSize_));

    GetDevice()->CreateConstantBufferView(&cbvDesc, handle->cpuHandle);

    SetDescriptorHandleInfo(std::move(handle));
    return true;
}

void *ConstantBufferResource::Map() {
    void *ptr = nullptr;
    if (GetResource()) {
        GetResource()->Map(0, nullptr, &ptr);
    }
    return ptr;
}

void ConstantBufferResource::Unmap() {
    if (GetResource()) {
        GetResource()->Unmap(0, nullptr);
    }
}

} // namespace KashipanEngine
