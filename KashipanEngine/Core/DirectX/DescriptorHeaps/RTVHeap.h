#pragma once
#include "Core/DirectX/DescriptorHeaps/DescriptorHeapBase.h"

namespace KashipanEngine {

/// @brief レンダーターゲットビュー用デスクリプタヒープクラス
class RTVHeap final : public DescriptorHeapBase {
public:
    RTVHeap(Passkey<DirectXCommon> key, ID3D12Device *device, UINT numDescriptors)
        : DescriptorHeapBase(key, device, D3D12_DESCRIPTOR_HEAP_TYPE_RTV, numDescriptors, D3D12_DESCRIPTOR_HEAP_FLAG_NONE) {}
    virtual ~RTVHeap() {
        LogScope scope;
        Log(Translation("instance.destroying"), LogSeverity::Debug);
        DescriptorHeapBase::~DescriptorHeapBase();
        Log(Translation("instance.destroyed"), LogSeverity::Debug);
    }

private:
    RTVHeap(const RTVHeap &) = delete;
    RTVHeap &operator=(const RTVHeap &) = delete;
    RTVHeap(RTVHeap &&) = delete;
    RTVHeap &operator=(RTVHeap &&) = delete;
};

} // namespace KashipanEngine