#pragma once
#include <d3d12.h>
#include "Graphics/Resources/IGraphicsResource.h"
#include "Core/DirectX/DescriptorHeaps/HeapDSV.h"

namespace KashipanEngine {

class DepthStencilResource final : public IGraphicsResource {
    static inline ID3D12GraphicsCommandList *sCommandList_ = nullptr;

public:
    /// @brief コマンドリスト設定
    static void SetCommandList(Passkey<DirectXCommon>, ID3D12GraphicsCommandList *commandList) {
        sCommandList_ = commandList;
    }

    /// @brief コンストラクタ
    /// @param width 横幅
    /// @param height 高さ
    /// @param format フォーマット
    /// @param clearDepth デプスクリア値
    /// @param clearStencil ステンシルクリア値
    /// @param dsvHeap DSVヒープ
    /// @param existingResource 既存リソース（nullptrの場合は新規作成）
    DepthStencilResource(UINT width, UINT height, DXGI_FORMAT format,
        FLOAT clearDepth, UINT8 clearStencil, DSVHeap *dsvHeap, ID3D12Resource *existingResource = nullptr);

    /// @brief リソース再生成
    /// @param width 横幅
    /// @param height 高さ
    /// @param format フォーマット
    /// @param clearDepth デプスクリア値
    /// @param clearStencil ステンシルクリア値
    /// @param existingResource 既存リソース（nullptrの場合は新規作成）
    /// @return 成功した場合はtrue、失敗した場合はfalseを返す
    bool Recreate(UINT width, UINT height, DXGI_FORMAT format,
        FLOAT clearDepth, UINT8 clearStencil, ID3D12Resource *existingResource = nullptr);

    /// @brief 深度ステンシルビューのクリア
    void ClearDepthStencilView() const;

    /// @brief DSVヒープの設定
    /// @param dsvHeap ヒープ
    void SetHeap(DSVHeap *dsvHeap) { dsvHeap_ = dsvHeap; }

private:
    bool Initialize(UINT width, UINT height, DXGI_FORMAT format, FLOAT clearDepth, UINT8 clearStencil, ID3D12Resource *existingResource);

    UINT width_ = 0;
    UINT height_ = 0;
    DXGI_FORMAT format_ = DXGI_FORMAT_D24_UNORM_S8_UINT;
    FLOAT clearDepth_ = 1.0f;
    UINT8 clearStencil_ = 0;
    DSVHeap *dsvHeap_ = nullptr;
};

} // namespace KashipanEngine
