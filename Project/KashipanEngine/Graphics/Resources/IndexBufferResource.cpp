#include "IndexBufferResource.h"

namespace KashipanEngine {

IndexBufferResource::IndexBufferResource(size_t byteSize, DXGI_FORMAT indexFormat, const void *initialData, ID3D12Resource *existingResource)
    : IGraphicsResource(ResourceViewType::IBV) {
    Initialize(byteSize, indexFormat, initialData, existingResource);
}

bool IndexBufferResource::Recreate(size_t byteSize, DXGI_FORMAT indexFormat, const void *initialData, ID3D12Resource *existingResource) {
    ResetResourceForRecreate();
    return Initialize(byteSize, indexFormat, initialData, existingResource);
}

bool IndexBufferResource::Initialize(size_t byteSize, DXGI_FORMAT indexFormat, const void *initialData, ID3D12Resource *existingResource) {
    LogScope scope;
    if (!GetDevice()) {
        Log(Translation("engine.graphics.resource.create.device.null"), LogSeverity::Warning);
        return false;
    }

    bufferSize_ = byteSize;
    indexFormat_ = indexFormat;

    D3D12_HEAP_PROPERTIES heapProps = {};
    heapProps.Type = D3D12_HEAP_TYPE_UPLOAD;

    D3D12_RESOURCE_DESC bufDesc = {};
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
        CreateResource(L"Index Buffer Resource", &heapProps, D3D12_HEAP_FLAG_NONE, &bufDesc, nullptr);
        if (!GetResource()) {
            return false;
        }
    }
    if (initialData) {
        void *dst = nullptr;
        GetResource()->Map(0, nullptr, &dst);
        memcpy(dst, initialData, bufferSize_);
        GetResource()->Unmap(0, nullptr);
    }

    return true;
}

D3D12_INDEX_BUFFER_VIEW IndexBufferResource::GetView() const {
    D3D12_INDEX_BUFFER_VIEW view{};
    if (GetResource()) {
        view.BufferLocation = GetResource()->GetGPUVirtualAddress();
        view.Format = indexFormat_;
        view.SizeInBytes = static_cast<UINT>(bufferSize_);
    }
    return view;
}

void *IndexBufferResource::Map() {
    void *ptr = nullptr;
    if (GetResource()) {
        GetResource()->Map(0, nullptr, &ptr);
    }
    return ptr;
}

void IndexBufferResource::Unmap() {
    if (GetResource()) {
        GetResource()->Unmap(0, nullptr);
    }
}

} // namespace KashipanEngine
