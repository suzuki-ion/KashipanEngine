#pragma once
#include <vector>
#include <memory>
#include <d3d12.h>
#include <wrl.h>

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
    virtual ~DescriptorHeapBase();

    /// @brief ヒープの種類を取得
    [[nodiscard]] D3D12_DESCRIPTOR_HEAP_TYPE GetType() const noexcept { return type_; }
    /// @brief デスクリプタ数を取得
    [[nodiscard]] UINT GetNumDescriptors() const noexcept { return numDescriptors_; }
    /// @brief シェーダー可視かどうかを取得
    [[nodiscard]] bool IsShaderVisible() const noexcept { return isShaderVisible_; }
    /// @brief デスクリプタヒープを取得
    [[nodiscard]] ID3D12DescriptorHeap *GetDescriptorHeap() const noexcept { return descriptorHeap_.Get(); }

    /// @brief デスクリプタハンドルを取得
    [[nodiscard]] std::unique_ptr<DescriptorHandleInfo> AllocateDescriptorHandle();
    /// @brief デスクリプタハンドルを解放
    void FreeDescriptorHandle(Passkey<DescriptorHandleInfo>, UINT index);

protected:
    /// @brief 派生クラス限定コンストラクタ
    /// @param device D3D12デバイス
    /// @param type デスクリプタヒープタイプ
    /// @param numDescriptors デスクリプタ数
    /// @param flags デスクリプタヒープフラグ
    DescriptorHeapBase(ID3D12Device *device, D3D12_DESCRIPTOR_HEAP_TYPE type, UINT numDescriptors, D3D12_DESCRIPTOR_HEAP_FLAGS flags);

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
