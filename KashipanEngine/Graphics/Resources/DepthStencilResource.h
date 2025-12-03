#pragma once
#include <d3d12.h>
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
    DepthStencilResource(UINT width, UINT height, DXGI_FORMAT format,
        FLOAT clearDepth, UINT8 clearStencil, ID3D12Resource *existingResource = nullptr);

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

private:
    bool Initialize(UINT width, UINT height, DXGI_FORMAT format, FLOAT clearDepth, UINT8 clearStencil, ID3D12Resource *existingResource);

    UINT width_ = 0;
    UINT height_ = 0;
    DXGI_FORMAT format_ = DXGI_FORMAT_D24_UNORM_S8_UINT;
    FLOAT clearDepth_ = 1.0f;
    UINT8 clearStencil_ = 0;
};

} // namespace KashipanEngine
