#include "RWStructuredBufferResource.h"

namespace KashipanEngine {

RWStructuredBufferResource::RWStructuredBufferResource(size_t elementStride, size_t elementCount)
    : IGraphicsResource(ResourceViewType::UAV) {
    Initialize(elementStride, elementCount);
}

bool RWStructuredBufferResource::Recreate(size_t elementStride, size_t elementCount) {
    ResetResourceForRecreate();
    return Initialize(elementStride, elementCount);
}

bool RWStructuredBufferResource::Initialize(size_t elementStride, size_t elementCount) {
    LogScope scope;
    if (!GetDevice()) {
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
    heapProps.Type = D3D12_HEAP_TYPE_DEFAULT;

    D3D12_RESOURCE_DESC bufDesc{};
    bufDesc.Dimension = D3D12_RESOURCE_DIMENSION_BUFFER;
    bufDesc.Width = bufferSize_;
    bufDesc.Height = 1;
    bufDesc.DepthOrArraySize = 1;
    bufDesc.MipLevels = 1;
    bufDesc.Layout = D3D12_TEXTURE_LAYOUT_ROW_MAJOR;
    bufDesc.SampleDesc.Count = 1;
    bufDesc.Flags = D3D12_RESOURCE_FLAG_ALLOW_UNORDERED_ACCESS;

    // Computeシェーダーの書き込み(UAV)と後続描画パスでの頂点バッファ読み取り(VBV)を
    // ルートディスクリプタ経由(SetComputeRootUnorderedAccessView)で切り替えるため両状態を保持する
    ClearTransitionStates();
    AddTransitionState(D3D12_RESOURCE_STATE_UNORDERED_ACCESS);
    AddTransitionState(D3D12_RESOURCE_STATE_VERTEX_AND_CONSTANT_BUFFER);

    CreateResource(L"RW Structured Buffer Resource", &heapProps, D3D12_HEAP_FLAG_NONE, &bufDesc, nullptr);
    if (!GetResource()) {
        return false;
    }

    return true;
}

D3D12_VERTEX_BUFFER_VIEW RWStructuredBufferResource::GetView(UINT stride) const {
    D3D12_VERTEX_BUFFER_VIEW view{};
    if (GetResource()) {
        view.BufferLocation = GetResource()->GetGPUVirtualAddress();
        view.StrideInBytes = stride;
        view.SizeInBytes = static_cast<UINT>(bufferSize_);
    }
    return view;
}

} // namespace KashipanEngine
