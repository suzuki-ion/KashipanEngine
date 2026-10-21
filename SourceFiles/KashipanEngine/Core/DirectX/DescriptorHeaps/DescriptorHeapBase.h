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

    /// @brief デスクリプタハンドルを取得（汎用プール：予約レンジ以降から払い出す）
    [[nodiscard]] std::unique_ptr<DescriptorHandleInfo> AllocateDescriptorHandle();
    /// @brief 予約レンジ（ヒープ先頭 [0, reservedCount) ）からデスクリプタハンドルを取得
    /// @details 予約レンジはインデックス0起点で確保されるため、レンジ全体を指す
    ///          デスクリプタテーブルのベースハンドルは常にヒープ先頭ハンドルと一致する
    ///          （GetReservedRangeBaseGpuHandle参照）。バインドレス用途（例：テクスチャ配列）に使う
    [[nodiscard]] std::unique_ptr<DescriptorHandleInfo> AllocateReservedDescriptorHandle();
    /// @brief デスクリプタハンドルを解放
    void FreeDescriptorHandle(Passkey<DescriptorHandleInfo>, UINT index);
    /// @brief 予約レンジ（ヒープ先頭）のGPUデスクリプタハンドルを取得
    /// @details 予約レンジが0件の場合は無効なハンドル（ptr==0）を返す
    [[nodiscard]] D3D12_GPU_DESCRIPTOR_HANDLE GetReservedRangeBaseGpuHandle() const noexcept;
    /// @brief 予約レンジのデスクリプタ数を取得
    [[nodiscard]] UINT GetReservedCount() const noexcept { return reservedCount_; }

protected:
    /// @brief 派生クラス限定コンストラクタ
    /// @param device D3D12デバイス
    /// @param type デスクリプタヒープタイプ
    /// @param numDescriptors デスクリプタ数（予約レンジを含めた総数）
    /// @param flags デスクリプタヒープフラグ
    /// @param reservedCount ヒープ先頭 [0, reservedCount) を予約レンジとして分離する数（既定0＝予約なし）
    DescriptorHeapBase(ID3D12Device *device, D3D12_DESCRIPTOR_HEAP_TYPE type, UINT numDescriptors, D3D12_DESCRIPTOR_HEAP_FLAGS flags, UINT reservedCount = 0);

private:
    DescriptorHeapBase(const DescriptorHeapBase &) = delete;
    DescriptorHeapBase &operator=(const DescriptorHeapBase &) = delete;
    DescriptorHeapBase(DescriptorHeapBase &&) = delete;
    DescriptorHeapBase &operator=(DescriptorHeapBase &&) = delete;

    [[nodiscard]] std::unique_ptr<DescriptorHandleInfo> AllocateFrom(std::vector<uint32_t> &pool);

    ID3D12Device *device_;
    Microsoft::WRL::ComPtr<ID3D12DescriptorHeap> descriptorHeap_;
    D3D12_DESCRIPTOR_HEAP_TYPE type_;
    UINT numDescriptors_;
    UINT reservedCount_;
    bool isShaderVisible_;

    /// @brief 汎用プール（[reservedCount_, numDescriptors_)）の空きインデックス
    std::vector<uint32_t> freeIndices_;
    /// @brief 予約プール（[0, reservedCount_)）の空きインデックス
    std::vector<uint32_t> freeReservedIndices_;
};

} // namespace KashipanEngine
