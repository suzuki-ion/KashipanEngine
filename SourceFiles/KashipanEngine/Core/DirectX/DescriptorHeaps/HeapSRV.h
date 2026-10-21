#pragma once
#include "Core/DirectX/DescriptorHeaps/DescriptorHeapBase.h"
#include "Utilities/Translation.h"

namespace KashipanEngine {

/// @brief レンダーターゲットビュー用デスクリプタヒープクラス
class HeapSRV final : public DescriptorHeapBase {
public:
    /// @param numDescriptors デスクリプタ数（予約レンジを含めた総数）
    /// @param reservedCount ヒープ先頭 [0, reservedCount) をテクスチャ等のバインドレス用途に予約する数（既定0）
    HeapSRV(Passkey<DirectXCommon>, ID3D12Device *device, UINT numDescriptors, UINT reservedCount = 0)
        : DescriptorHeapBase(device, D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV, numDescriptors, D3D12_DESCRIPTOR_HEAP_FLAG_SHADER_VISIBLE, reservedCount) {}
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