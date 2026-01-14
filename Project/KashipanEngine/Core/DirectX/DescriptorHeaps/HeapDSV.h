#pragma once
#include "Core/DirectX/DescriptorHeaps/DescriptorHeapBase.h"

namespace KashipanEngine {

/// @brief 深度ステンシルビュー用デスクリプタヒープクラス
class HeapDSV final : public DescriptorHeapBase {
public:
    HeapDSV(Passkey<DirectXCommon>, ID3D12Device *device, UINT numDescriptors)
        : DescriptorHeapBase(device, D3D12_DESCRIPTOR_HEAP_TYPE_DSV, numDescriptors, D3D12_DESCRIPTOR_HEAP_FLAG_NONE) {}
    virtual ~HeapDSV() {
        LogScope scope;
        Log(Translation("instance.destroying"), LogSeverity::Debug);
        DescriptorHeapBase::~DescriptorHeapBase();
        Log(Translation("instance.destroyed"), LogSeverity::Debug);
    }

private:
    HeapDSV(const HeapDSV &) = delete;
    HeapDSV &operator=(const HeapDSV &) = delete;
    HeapDSV(HeapDSV &&) = delete;
    HeapDSV &operator=(HeapDSV &&) = delete;
};
using DSVHeap = HeapDSV;

} // namespace KashipanEngine