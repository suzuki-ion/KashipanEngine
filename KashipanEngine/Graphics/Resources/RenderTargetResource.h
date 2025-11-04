#pragma once
#include <d3d12.h>
#include <memory>
#include "Graphics/Resources/IGraphicsResource.h"
#include "Core/DirectX/DescriptorHeaps/HeapRTV.h"

namespace KashipanEngine {

class DX12SwapChain;

class RenderTargetResource final : public IGraphicsResource {
    static inline ID3D12GraphicsCommandList *sCommandList_ = nullptr;
    static inline FLOAT sDefaultClearColor_[4] = { 0.0f, 0.0f, 0.0f, 0.0f };

public:
    /// @brief コマンドリスト設定
    static void SetCommandList(Passkey<DirectXCommon>, ID3D12GraphicsCommandList *commandList) {
        sCommandList_ = commandList;
    }
    /// @brief デフォルトのクリアカラー設定
    /// @param clearColor クリアカラー配列（RGBA）
    static void SetDefaultClearColor(Passkey<DirectXCommon>, const FLOAT clearColor[4]) {
        sDefaultClearColor_[0] = clearColor[0];
        sDefaultClearColor_[1] = clearColor[1];
        sDefaultClearColor_[2] = clearColor[2];
        sDefaultClearColor_[3] = clearColor[3];
    }
    /// @brief コンストラクタ
    /// @param width 横幅
    /// @param height 高さ
    /// @param format フォーマット
    /// @param rtvHeap RTVヒープ
    /// @param existingResource 既存リソース（nullptrの場合は新規作成）
    /// @param clearColor クリアカラー配列（RGBA）
    RenderTargetResource(UINT width, UINT height, DXGI_FORMAT format, RTVHeap *rtvHeap,
        ID3D12Resource *existingResource = nullptr, const FLOAT clearColor[4] = sDefaultClearColor_);

    /// @brief リソース再生成
    /// @param width 横幅
    /// @param height 高さ
    /// @param format フォーマット
    /// @param existingResource 既存リソース（nullptrの場合は新規作成）
    /// @param clearColor クリアカラー配列（RGBA）
    /// @return 成功した場合はtrue、失敗した場合はfalseを返す
    bool Recreate(UINT width, UINT height, DXGI_FORMAT format,
        ID3D12Resource *existingResource = nullptr, const FLOAT clearColor[4] = sDefaultClearColor_);

    /// @brief レンダーターゲットビューのクリア
    void ClearRenderTargetView() const;

    /// @brief RTVヒープの設定
    /// @param rtvHeap ヒープ
    void SetHeap(RTVHeap *rtvHeap) { rtvHeap_ = rtvHeap; }

    UINT GetWidth() const { return width_; }
    UINT GetHeight() const { return height_; }
    DXGI_FORMAT GetFormat() const { return format_; }

private:
    bool Initialize(UINT width, UINT height, DXGI_FORMAT format, ID3D12Resource *existingResource, const FLOAT clearColor[4]);

    UINT width_ = 0;
    UINT height_ = 0;
    DXGI_FORMAT format_ = DXGI_FORMAT_R8G8B8A8_UNORM;
    FLOAT clearColor_[4] { 0.f, 0.f, 0.f, 0.f };
    RTVHeap *rtvHeap_ = nullptr;
};

} // namespace KashipanEngine
