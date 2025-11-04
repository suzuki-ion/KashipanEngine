#pragma once
#include <d3d12.h>
#include <memory>
#include "Graphics/Resources/IGraphicsResource.h"
#include "Core/DirectX/DescriptorHeaps/HeapSRV.h"

namespace KashipanEngine {

class UnorderedAccessResource final : public IGraphicsResource {
public:
    /// @brief コンストラクタ
    /// @param width 横幅
    /// @param height 高さ
    /// @param format フォーマット
    /// @param srvUavHeap SRV/UAV用ヒープ
    /// @param existingResource 既存リソース（nullptrの場合は新規作成）
    UnorderedAccessResource(UINT width, UINT height, DXGI_FORMAT format, UAVHeap *uavHeap, ID3D12Resource *existingResource = nullptr);

    /// @brief リソース再生成
    /// @param width 横幅
    /// @param height 高さ
    /// @param format フォーマット
    /// @param existingResource 既存リソース（nullptrの場合は新規作成）
    /// @return 成功した場合はtrue、失敗した場合はfalseを返す
    bool Recreate(UINT width, UINT height, DXGI_FORMAT format, ID3D12Resource *existingResource = nullptr);

    /// @brief UAVヒープの設定
    /// @param uavHeap UAVヒープ
    void SetHeap(UAVHeap *uavHeap) { uavHeap_ = uavHeap; }

private:
    bool Initialize(UINT width, UINT height, DXGI_FORMAT format, ID3D12Resource *existingResource = nullptr);

    UINT width_ = 0;
    UINT height_ = 0;
    DXGI_FORMAT format_ = DXGI_FORMAT_R8G8B8A8_UNORM;
    UAVHeap *uavHeap_ = nullptr;
};

} // namespace KashipanEngine
