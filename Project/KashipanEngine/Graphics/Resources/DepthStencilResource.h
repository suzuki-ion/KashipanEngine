#pragma once
#include <d3d12.h>
#include <memory>
#include <vector>
#include "Graphics/Resources/IGraphicsResource.h"
#include "Core/DirectX/DescriptorHeaps/HeapDSV.h"

namespace KashipanEngine {

class DepthStencilResource final : public IGraphicsResource {
public:
    /// @brief コンストラクタ
    /// @param width 横幅
    /// @param height 高さ
    /// @param format フォーマット
    /// @param clearDepth デプスクリア値
    /// @param clearStencil ステンシルクリア値
    /// @param existingResource 既存リソース（nullptrの場合は新規作成）
    /// @param createSrv SRV を作成するか（シャドウマップ用）
    /// @param srvFormat SRV 用フォーマット（未指定の場合は format から推定）
    /// @param arraySize 配列サイズ（2以上の場合はTexture2DArrayとして作成され、スライス別DSVと配列SRVを持つ）
    DepthStencilResource(UINT width, UINT height, DXGI_FORMAT format,
        FLOAT clearDepth, UINT8 clearStencil,
        ID3D12Resource *existingResource = nullptr,
        bool createSrv = false,
        DXGI_FORMAT srvFormat = DXGI_FORMAT_UNKNOWN,
        UINT arraySize = 1);

    /// @brief リソース再生成
    /// @param width 横幅
    /// @param height 高さ
    /// @param format フォーマット
    /// @param clearDepth デプスクリア値
    /// @param clearStencil ステンシルクリア値
    /// @param existingResource 既存リソース（nullptrの場合は新規作成）
    /// @param createSrv SRV を作成するか（シャドウマップ用）
    /// @param srvFormat SRV 用フォーマット（未指定の場合は format から推定）
    /// @param arraySize 配列サイズ（2以上の場合はTexture2DArrayとして作成される）
    /// @return 成功した場合はtrue、失敗した場合はfalseを返す
    bool Recreate(UINT width, UINT height, DXGI_FORMAT format,
        FLOAT clearDepth, UINT8 clearStencil,
        ID3D12Resource *existingResource = nullptr,
        bool createSrv = false,
        DXGI_FORMAT srvFormat = DXGI_FORMAT_UNKNOWN,
        UINT arraySize = 1);

    /// @brief 深度ステンシルビューのクリア（配列の場合は全スライスを一括クリアする）
    void ClearDepthStencilView() const;

    /// @brief 配列サイズを取得
    UINT GetArraySize() const noexcept { return arraySize_; }

    /// @brief 指定スライスのDSVハンドルを取得（配列でない場合は通常のDSVを返す）
    D3D12_CPU_DESCRIPTOR_HANDLE GetSliceDsvHandle(UINT slice) const;

    /// @brief SRV を持つか
    bool HasSrv() const noexcept { return srvHandleInfo_ != nullptr; }

    /// @brief SRV の GPU ハンドル取得（未作成の場合は空）
    D3D12_GPU_DESCRIPTOR_HANDLE GetSrvGPUHandle() const noexcept { return srvHandleInfo_ ? srvHandleInfo_->gpuHandle : D3D12_GPU_DESCRIPTOR_HANDLE{}; }

    /// @brief SRV の CPU ハンドル取得（未作成の場合は空）
    D3D12_CPU_DESCRIPTOR_HANDLE GetSrvCPUHandle() const noexcept { return srvHandleInfo_ ? srvHandleInfo_->cpuHandle : D3D12_CPU_DESCRIPTOR_HANDLE{}; }

    UINT GetWidth() const noexcept { return width_; }
    UINT GetHeight() const noexcept { return height_; }

    /// @brief SRV を持つ場合、シェーダーからサンプル可能な状態へ遷移
    bool TransitionToShaderResource() {
        if (!HasSrv()) return false;
        return TransitionTo(D3D12_RESOURCE_STATE_NON_PIXEL_SHADER_RESOURCE | D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE);
    }

private:
    static DXGI_FORMAT GuessSrvFormatFromDsvFormat(DXGI_FORMAT dsvFormat) noexcept;

    bool Initialize(UINT width, UINT height, DXGI_FORMAT format, FLOAT clearDepth, UINT8 clearStencil,
        ID3D12Resource *existingResource,
        bool createSrv,
        DXGI_FORMAT srvFormat,
        UINT arraySize);

    void CreateSrvInternal(DXGI_FORMAT srvFormat);

    UINT width_ = 0;
    UINT height_ = 0;
    UINT arraySize_ = 1;
    DXGI_FORMAT format_ = DXGI_FORMAT_D24_UNORM_S8_UINT;
    FLOAT clearDepth_ = 1.0f;
    UINT8 clearStencil_ = 0;

    std::unique_ptr<DescriptorHandleInfo> srvHandleInfo_;
    /// @brief 配列の場合のスライス別DSVハンドル（arraySize_ >= 2 の場合のみ使用）
    std::vector<std::unique_ptr<DescriptorHandleInfo>> sliceDsvHandles_;
};

} // namespace KashipanEngine
