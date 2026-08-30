#pragma once
#include <d3d12.h>
#include <memory>
#include "Graphics/Resources/IGraphicsResource.h"
#include "Core/DirectX/DescriptorHeaps/HeapSRV.h"

namespace KashipanEngine {

class TextureManager;
class RenderTargetResource;

class ShaderResourceResource final : public IGraphicsResource {
public:
    /// @brief コンストラクタ
    /// @param width 横幅
    /// @param height 高さ
    /// @param format フォーマット
    /// @param flags リソースフラグ
    /// @param existingResource 既存リソース（nullptrの場合は新規作成）
    /// @param initialState 初期状態（デフォルトは PS で使用可能な状態）
    /// @param mipLevels ミップレベル数
    /// @param useReservedRange trueの場合、SRVヒープ先頭のテクスチャ専用予約レンジ（バインドレステーブル用）から
    ///        デスクリプタを確保する。マテリアルのテクスチャとして参照され得るテクスチャ（TextureManager/
    ///        FontManagerのアトラス/ScreenBuffer/ShadowMapBuffer/VideoTexture/GifTexture等）はtrueにすること
    ShaderResourceResource(
        UINT width,
        UINT height,
        DXGI_FORMAT format,
        D3D12_RESOURCE_FLAGS flags = D3D12_RESOURCE_FLAG_NONE,
        ID3D12Resource *existingResource = nullptr,
        D3D12_RESOURCE_STATES initialState = D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE,
        UINT mipLevels = 1,
        UINT arraySize = 1,
        const D3D12_SHADER_RESOURCE_VIEW_DESC *externalSrvDesc = nullptr,
        bool useReservedRange = false);

    /// @brief RenderTargetResource の描画結果を SRV として参照するためのコンストラクタ
    /// @param renderTarget 参照元レンダーターゲット（内部リソースを共有）
    /// @param initialState 初期状態（デフォルトは PS で使用可能な状態）
    /// @param mipLevels ミップレベル数
    /// @param useReservedRange 上記コンストラクタと同様（ScreenBuffer等、マテリアルのテクスチャとして参照され得る場合はtrue）
    explicit ShaderResourceResource(
        RenderTargetResource* renderTarget,
        D3D12_RESOURCE_STATES initialState = D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE,
        UINT mipLevels = 1,
        const D3D12_SHADER_RESOURCE_VIEW_DESC *externalSrvDesc = nullptr,
        bool useReservedRange = false);

    /// @brief リソース再生成
    /// @param width 横幅
    /// @param height 高さ
    /// @param format フォーマット
    /// @param flags リソースフラグ
    /// @param existingResource 既存リソース（nullptrの場合は新規作成）
    /// @param initialState 初期状態
    /// @param mipLevels ミップレベル数
    /// @param useReservedRange 上記コンストラクタと同様
    /// @return 成功した場合はtrue、失敗した場合はfalseを返す
    bool Recreate(
        UINT width,
        UINT height,
        DXGI_FORMAT format,
        D3D12_RESOURCE_FLAGS flags = D3D12_RESOURCE_FLAG_NONE,
        ID3D12Resource *existingResource = nullptr,
        D3D12_RESOURCE_STATES initialState = D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE,
        UINT mipLevels = 1,
        UINT arraySize = 1,
        const D3D12_SHADER_RESOURCE_VIEW_DESC *externalSrvDesc = nullptr,
        bool useReservedRange = false);

    /// @brief デスクリプタ情報取得
    DescriptorHandleInfo *GetDescriptorHandleInfoForTextureManager(Passkey<TextureManager>) const { return GetDescriptorHandleInfo(); }

private:
    bool Initialize(
        UINT width,
        UINT height,
        DXGI_FORMAT format,
        D3D12_RESOURCE_FLAGS flags = D3D12_RESOURCE_FLAG_NONE,
        ID3D12Resource *existingResource = nullptr,
        D3D12_RESOURCE_STATES initialState = D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE,
        UINT mipLevels = 1,
        UINT arraySize = 1,
        const D3D12_SHADER_RESOURCE_VIEW_DESC *externalSrvDesc = nullptr,
        bool useReservedRange = false);

    UINT width_ = 0;
    UINT height_ = 0;
    DXGI_FORMAT format_ = DXGI_FORMAT_R8G8B8A8_UNORM;
    D3D12_RESOURCE_FLAGS flags_ = D3D12_RESOURCE_FLAG_NONE;
    UINT mipLevels_ = 1;
};

} // namespace KashipanEngine
