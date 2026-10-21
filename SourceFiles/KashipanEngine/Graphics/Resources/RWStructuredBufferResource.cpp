#include "RWStructuredBufferResource.h"
#include "Utilities/Translation.h"

namespace KashipanEngine {

RWStructuredBufferResource::RWStructuredBufferResource(size_t elementStride, size_t elementCount, bool createSrv)
    : IGraphicsResource(ResourceViewType::UAV) {
    Initialize(elementStride, elementCount, createSrv);
}

bool RWStructuredBufferResource::Recreate(size_t elementStride, size_t elementCount, bool createSrv) {
    ResetResourceForRecreate();
    return Initialize(elementStride, elementCount, createSrv);
}

bool RWStructuredBufferResource::Initialize(size_t elementStride, size_t elementCount, bool createSrv) {
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

    // Computeシェーダーの書き込み(UAV)と後続パスでの読み取りを切り替えるため両状態を保持する。
    // createSrv=falseの場合（スキニング用途）は後続を頂点バッファ(VBV)として読み、
    // createSrv=trueの場合（ライトカリング等）は後続をStructuredBuffer(SRV)として読む
    ClearTransitionStates();
    AddTransitionState(D3D12_RESOURCE_STATE_UNORDERED_ACCESS);
    if (createSrv) {
        AddTransitionState(D3D12_RESOURCE_STATE_NON_PIXEL_SHADER_RESOURCE | D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE);
    } else {
        AddTransitionState(D3D12_RESOURCE_STATE_VERTEX_AND_CONSTANT_BUFFER);
    }

    CreateResource(L"RW Structured Buffer Resource", &heapProps, D3D12_HEAP_FLAG_NONE, &bufDesc, nullptr);
    if (!GetResource()) {
        return false;
    }

    if (createSrv) {
        auto *srvHeap = GetSRVHeap();
        if (srvHeap) {
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
        }
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
