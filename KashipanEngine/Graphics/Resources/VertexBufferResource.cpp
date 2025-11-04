#include "VertexBufferResource.h"

namespace KashipanEngine {

VertexBufferResource::VertexBufferResource(size_t byteSize, const void *initialData, ID3D12Resource *existingResource)
    : IGraphicsResource(ResourceViewType::VBV) {
    Initialize(byteSize, initialData, existingResource);
}

bool VertexBufferResource::Recreate(size_t byteSize, const void *initialData, ID3D12Resource *existingResource) {
    ResetResourceForRecreate();
    return Initialize(byteSize, initialData, existingResource);
}

bool VertexBufferResource::Initialize(size_t byteSize, const void *initialData, ID3D12Resource *existingResource) {
    LogScope scope;
    if (!GetDevice()) {
        Log(Translation("engine.graphics.resource.create.device.null"), LogSeverity::Warning);
        return false;
    }

    bufferSize_ = byteSize;

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
        CreateResource(L"Vertex Buffer Resource", &heapProps, D3D12_HEAP_FLAG_NONE, &bufDesc, nullptr);
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

D3D12_VERTEX_BUFFER_VIEW VertexBufferResource::GetView(UINT stride) const {
    D3D12_VERTEX_BUFFER_VIEW view{};
    if (GetResource()) {
        view.BufferLocation = GetResource()->GetGPUVirtualAddress();
        view.StrideInBytes = stride;
        view.SizeInBytes = static_cast<UINT>(bufferSize_);
    }
    return view;
}

void *VertexBufferResource::Map() {
    void *ptr = nullptr;
    if (GetResource()) {
        GetResource()->Map(0, nullptr, &ptr);
    }
    return ptr;
}

void VertexBufferResource::Unmap() {
    if (GetResource()) {
        GetResource()->Unmap(0, nullptr);
    }
}

} // namespace KashipanEngine
