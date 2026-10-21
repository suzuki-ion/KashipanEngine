#pragma once
#include "Core/DirectX/DescriptorHeaps/DescriptorHeapBase.h"
#include "Utilities/Translation.h"

namespace KashipanEngine {

/// @brief サンプラー用デスクリプタヒープクラス
class HeapSampler final : public DescriptorHeapBase {
public:
    /// @param numDescriptors デスクリプタ数（予約レンジを含めた総数）
    /// @param reservedCount ヒープ先頭 [0, reservedCount) を既定サンプラー等のバインドレス用途に予約する数（既定0）
    HeapSampler(Passkey<DirectXCommon>, ID3D12Device *device, UINT numDescriptors, UINT reservedCount = 0)
        : DescriptorHeapBase(device, D3D12_DESCRIPTOR_HEAP_TYPE_SAMPLER, numDescriptors, D3D12_DESCRIPTOR_HEAP_FLAG_SHADER_VISIBLE, reservedCount) {}
    virtual ~HeapSampler() {
        LogScope scope;
        Log(Translation("instance.destroying"), LogSeverity::Debug);
        DescriptorHeapBase::~DescriptorHeapBase();
        Log(Translation("instance.destroyed"), LogSeverity::Debug);
    }

private:
    HeapSampler(const HeapSampler &) = delete;
    HeapSampler &operator=(const HeapSampler &) = delete;
    HeapSampler(HeapSampler &&) = delete;
    HeapSampler &operator=(HeapSampler &&) = delete;
};
using SamplerHeap = HeapSampler;

} // namespace KashipanEngine
