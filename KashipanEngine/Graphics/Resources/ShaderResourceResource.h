#pragma once
#include <d3d12.h>
#include <memory>
#include "Graphics/Resources/IGraphicsResource.h"
#include "Core/DirectX/DescriptorHeaps/HeapSRV.h"

namespace KashipanEngine {

class ShaderResourceResource final : public IGraphicsResource {
public:
    /// @brief コンストラクタ
    /// @param width 横幅
    /// @param height 高さ
    /// @param format フォーマット
    /// @param srvHeap SRVヒープ
    /// @param flags リソースフラグ
    /// @param existingResource 既存リソース（nullptrの場合は新規作成）
    ShaderResourceResource(UINT width, UINT height, DXGI_FORMAT format, SRVHeap *srvHeap, D3D12_RESOURCE_FLAGS flags = D3D12_RESOURCE_FLAG_NONE, ID3D12Resource *existingResource = nullptr);

    /// @brief リソース再生成
    /// @param width 横幅
    /// @param height 高さ
    /// @param format フォーマット
    /// @param flags リソースフラグ
    /// @param existingResource 既存リソース（nullptrの場合は新規作成）
    /// @return 成功した場合はtrue、失敗した場合はfalseを返す
    bool Recreate(UINT width, UINT height, DXGI_FORMAT format, D3D12_RESOURCE_FLAGS flags = D3D12_RESOURCE_FLAG_NONE, ID3D12Resource *existingResource = nullptr);

    /// @brief SRVヒープの設定
    /// @param srvHeap SRVヒープ
    void SetHeap(SRVHeap *srvHeap) { srvHeap_ = srvHeap; }

private:
    bool Initialize(UINT width, UINT height, DXGI_FORMAT format, D3D12_RESOURCE_FLAGS flags = D3D12_RESOURCE_FLAG_NONE, ID3D12Resource *existingResource = nullptr);

    UINT width_ = 0;
    UINT height_ = 0;
    DXGI_FORMAT format_ = DXGI_FORMAT_R8G8B8A8_UNORM;
    D3D12_RESOURCE_FLAGS flags_ = D3D12_RESOURCE_FLAG_NONE;
    SRVHeap *srvHeap_ = nullptr;
};

} // namespace KashipanEngine
