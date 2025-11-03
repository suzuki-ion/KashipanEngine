#pragma once
#include "Core/DirectX/DescriptorHeaps/DescriptorHeapBase.h"

namespace KashipanEngine {

/// @brief 深度ステンシルビュー用デスクリプタヒープクラス
class DSVHeap final : public DescriptorHeapBase {
public:
    DSVHeap(Passkey<DirectXCommon> key, ID3D12Device *device, UINT numDescriptors)
        : DescriptorHeapBase(key, device, D3D12_DESCRIPTOR_HEAP_TYPE_DSV, numDescriptors, D3D12_DESCRIPTOR_HEAP_FLAG_NONE) {}
    virtual ~DSVHeap() {
        LogScope scope;
        Log(Translation("instance.destroying"), LogSeverity::Debug);
        DescriptorHeapBase::~DescriptorHeapBase();
        Log(Translation("instance.destroyed"), LogSeverity::Debug);
    }

private:
    DSVHeap(const DSVHeap &) = delete;
    DSVHeap &operator=(const DSVHeap &) = delete;
    DSVHeap(DSVHeap &&) = delete;
    DSVHeap &operator=(DSVHeap &&) = delete;
};

} // namespace KashipanEngine