#pragma once
#include "Core/DirectX/DescriptorHeaps/DescriptorHeapBase.h"

namespace KashipanEngine {

/// @brief レンダーターゲットビュー用デスクリプタヒープクラス
class HeapRTV final : public DescriptorHeapBase {
public:
    HeapRTV(Passkey<DirectXCommon>, ID3D12Device *device, UINT numDescriptors)
        : DescriptorHeapBase(device, D3D12_DESCRIPTOR_HEAP_TYPE_RTV, numDescriptors, D3D12_DESCRIPTOR_HEAP_FLAG_NONE) {}
    virtual ~HeapRTV() {
        LogScope scope;
        Log(Translation("instance.destroying"), LogSeverity::Debug);
        DescriptorHeapBase::~DescriptorHeapBase();
        Log(Translation("instance.destroyed"), LogSeverity::Debug);
    }

private:
    HeapRTV(const HeapRTV &) = delete;
    HeapRTV &operator=(const HeapRTV &) = delete;
    HeapRTV(HeapRTV &&) = delete;
    HeapRTV &operator=(HeapRTV &&) = delete;
};
using RTVHeap = HeapRTV;

} // namespace KashipanEngine