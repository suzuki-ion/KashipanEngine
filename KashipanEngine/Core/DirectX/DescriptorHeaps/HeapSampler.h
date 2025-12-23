#pragma once
#include "Core/DirectX/DescriptorHeaps/DescriptorHeapBase.h"

namespace KashipanEngine {

/// @brief サンプラー用デスクリプタヒープクラス
class HeapSampler final : public DescriptorHeapBase {
public:
    HeapSampler(Passkey<DirectXCommon>, ID3D12Device *device, UINT numDescriptors)
        : DescriptorHeapBase(device, D3D12_DESCRIPTOR_HEAP_TYPE_SAMPLER, numDescriptors, D3D12_DESCRIPTOR_HEAP_FLAG_SHADER_VISIBLE) {}
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
