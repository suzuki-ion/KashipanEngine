#pragma once
#include <vector>
#include <memory>
#include <d3d12.h>
#include <wrl.h>
#include "Utilities/Passkeys.h"

namespace KashipanEngine {

class DirectXCommon;
class DescriptorHeapBase;

/// @brief デスクリプタハンドル情報構造体
struct DescriptorHandleInfo {
    DescriptorHandleInfo(Passkey<DescriptorHeapBase>, DescriptorHeapBase *owner,  UINT idx, D3D12_CPU_DESCRIPTOR_HANDLE cpuHdl, D3D12_GPU_DESCRIPTOR_HANDLE gpuHdl)
        : owner_(owner), index(idx), cpuHandle(cpuHdl), gpuHandle(gpuHdl) {}
    ~DescriptorHandleInfo();
    const UINT index;                               ///< デスクリプタインデックス
    const D3D12_CPU_DESCRIPTOR_HANDLE cpuHandle;    ///< CPUデスクリプタハンドル
    const D3D12_GPU_DESCRIPTOR_HANDLE gpuHandle;    ///< GPUデスクリプタハンドル

private:
    DescriptorHandleInfo(const DescriptorHandleInfo &) = delete;
    DescriptorHandleInfo &operator=(const DescriptorHandleInfo &) = delete;
    DescriptorHandleInfo(DescriptorHandleInfo &&) = delete;
    DescriptorHandleInfo &operator=(DescriptorHandleInfo &&) = delete;

    DescriptorHeapBase *owner_;
};

/// @brief デスクリプタヒープ基底クラス
class DescriptorHeapBase {
public:
    DescriptorHeapBase(Passkey<DirectXCommon>, ID3D12Device *device, D3D12_DESCRIPTOR_HEAP_TYPE type, UINT numDescriptors, D3D12_DESCRIPTOR_HEAP_FLAGS flags);
    virtual ~DescriptorHeapBase();

    /// @brief デスクリプタハンドルを取得
    [[nodiscard]] std::unique_ptr<DescriptorHandleInfo> AllocateDescriptorHandle();
    /// @brief デスクリプタハンドルを解放
    void FreeDescriptorHandle(Passkey<DescriptorHandleInfo>, UINT index);

private:
    DescriptorHeapBase(const DescriptorHeapBase &) = delete;
    DescriptorHeapBase &operator=(const DescriptorHeapBase &) = delete;
    DescriptorHeapBase(DescriptorHeapBase &&) = delete;
    DescriptorHeapBase &operator=(DescriptorHeapBase &&) = delete;

    ID3D12Device *device_;
    Microsoft::WRL::ComPtr<ID3D12DescriptorHeap> descriptorHeap_;
    D3D12_DESCRIPTOR_HEAP_TYPE type_;
    UINT numDescriptors_;
    bool isShaderVisible_;

    std::vector<uint32_t> freeIndices_;
};

} // namespace KashipanEngine
