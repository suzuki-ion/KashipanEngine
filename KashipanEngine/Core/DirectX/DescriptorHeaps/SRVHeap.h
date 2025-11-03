#pragma once
#include "Core/DirectX/DescriptorHeaps/DescriptorHeapBase.h"

namespace KashipanEngine {

/// @brief レンダーターゲットビュー用デスクリプタヒープクラス
class SRVHeap final : public DescriptorHeapBase {
public:
    SRVHeap(Passkey<DirectXCommon> key, ID3D12Device *device, UINT numDescriptors) 
        : DescriptorHeapBase(key, device, D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV, numDescriptors, D3D12_DESCRIPTOR_HEAP_FLAG_SHADER_VISIBLE) {}
    virtual ~SRVHeap() {
        LogScope scope;
        Log(Translation("instance.destroying"), LogSeverity::Debug);
        DescriptorHeapBase::~DescriptorHeapBase();
        Log(Translation("instance.destroyed"), LogSeverity::Debug);
    }

private:
    SRVHeap(const SRVHeap &) = delete;
    SRVHeap &operator=(const SRVHeap &) = delete;
    SRVHeap(SRVHeap &&) = delete;
    SRVHeap &operator=(SRVHeap &&) = delete;
};

} // namespace KashipanEngine