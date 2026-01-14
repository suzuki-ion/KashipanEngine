#pragma once
#include "Core/DirectX/DescriptorHeaps/DescriptorHeapBase.h"

namespace KashipanEngine {

/// @brief レンダーターゲットビュー用デスクリプタヒープクラス
class HeapSRV final : public DescriptorHeapBase {
public:
    HeapSRV(Passkey<DirectXCommon>, ID3D12Device *device, UINT numDescriptors) 
        : DescriptorHeapBase(device, D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV, numDescriptors, D3D12_DESCRIPTOR_HEAP_FLAG_SHADER_VISIBLE) {}
    virtual ~HeapSRV() {
        LogScope scope;
        Log(Translation("instance.destroying"), LogSeverity::Debug);
        DescriptorHeapBase::~DescriptorHeapBase();
        Log(Translation("instance.destroyed"), LogSeverity::Debug);
    }

private:
    HeapSRV(const HeapSRV &) = delete;
    HeapSRV &operator=(const HeapSRV &) = delete;
    HeapSRV(HeapSRV &&) = delete;
    HeapSRV &operator=(HeapSRV &&) = delete;
};
using HeapCBV = HeapSRV;
using HeapUAV = HeapSRV;
using SRVHeap = HeapSRV;
using CBVHeap = HeapSRV;
using UAVHeap = HeapSRV;

} // namespace KashipanEngine