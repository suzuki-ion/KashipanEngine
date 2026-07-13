#pragma once
#include <cstdint>
#include <memory>
#include <unordered_map>

#include "Core/DirectX/DX12Commands.h"
#include "Utilities/Passkeys.h"
#include "Graphics/Resources/DepthStencilResource.h"
#include "Graphics/IShaderTexture.h"
#include "Graphics/IRenderTarget.h"

namespace KashipanEngine {

class GameEngine;
class DirectXCommon;

/// @brief シャドウマップ生成用（深度のみ）オフスクリーンバッファ
class ShadowMapBuffer final : public IShaderTexture, public IRenderTarget {
public:
    /// @brief GameEngine から DirectXCommon を設定
    static void SetDirectXCommon(Passkey<GameEngine>, DirectXCommon *dx) { sDirectXCommon_ = dx; }
    /// @brief 全 ShadowMapBuffer 破棄
    static void AllDestroy(Passkey<GameEngine>);
    /// @brief 破棄要求済み ShadowMapBuffer をフレーム終端で実際に破棄する
    static void CommitDestroy(Passkey<GameEngine>);

    /// @brief ShadowMapBuffer 生成
    /// @param width シャドウマップ解像度
    /// @param height シャドウマップ解像度
    /// @param name 管理用の名前（空の場合は自動生成。TextureManagerにこの名前で登録される）
    /// @param depthFormat DSV 用フォーマット（例: DXGI_FORMAT_D32_FLOAT）
    /// @param srvFormat SRV 用フォーマット（例: DXGI_FORMAT_R32_FLOAT）
    static ShadowMapBuffer *Create(std::uint32_t width, std::uint32_t height,
        const std::string &name = "",
        DXGI_FORMAT depthFormat = DXGI_FORMAT_D32_FLOAT,
        DXGI_FORMAT srvFormat = DXGI_FORMAT_R32_FLOAT);
    /// @brief ポインタから存在確認
    static bool IsExist(ShadowMapBuffer *buffer);

    ShadowMapBuffer(const ShadowMapBuffer &) = delete;
    ShadowMapBuffer &operator=(const ShadowMapBuffer &) = delete;
    ShadowMapBuffer(ShadowMapBuffer &&) = delete;
    ShadowMapBuffer &operator=(ShadowMapBuffer &&) = delete;

    ~ShadowMapBuffer();

    /// @brief 指定バッファが破棄要求済みか
    bool IsPendingDestroy() const;
    /// @brief アプリ側から破棄要求（実体の破棄は CommitDestroy で行う。TextureManager への登録は即座に解除する）
    void DestroyNotify();

    //==================================================
    // IShaderTexture オーバーライド関数
    //==================================================

    std::uint32_t GetWidth() const noexcept override { return width_; }
    std::uint32_t GetHeight() const noexcept override { return height_; }
    D3D12_GPU_DESCRIPTOR_HANDLE GetSrvHandle() const noexcept override;

    //==================================================
    // IRenderTarget オーバーライド関数
    //==================================================

    RenderTargetKind GetRenderTargetKind() const noexcept override { return RenderTargetKind::ShadowMapBuffer; }
    std::string GetRenderTargetName() const override { return name_; }
    /// @brief 管理用の名前を設定（TextureManagerへの登録名も更新される）
    void SetRenderTargetName(const std::string &name);
    /// @brief TextureManagerに登録されているテクスチャハンドルを取得
    std::uint32_t GetTextureHandle() const noexcept { return textureHandle_; }
    std::uint32_t GetRenderTargetWidth() const noexcept override { return width_; }
    std::uint32_t GetRenderTargetHeight() const noexcept override { return height_; }
    bool IsRenderTargetAvailable() const noexcept override { return width_ > 0 && height_ > 0 && !this->IsPendingDestroy(); }

    void BeginDraw() override;
    void EndDraw() override;
    ID3D12GraphicsCommandList *GetCommandList() const override { return dx12Commands_ ? dx12Commands_->GetCommandList() : nullptr; }

    /// @brief バッファサイズを変更する（GPUリソースを新しいサイズで作り直す）
    /// @details TextureManagerへの登録名・ハンドルはそのまま維持される（サイズはこのオブジェクトから
    ///          毎回取得されるため再登録は不要）。BeginDraw/EndDraw の外側から呼ぶこと。
    /// @return 成功した場合はtrue、失敗した場合（未初期化・サイズが0等）はfalseを返す
    bool Resize(std::uint32_t width, std::uint32_t height);

    //==================================================
    // リソース取得
    //==================================================

    DepthStencilResource *GetDepthStencil() const noexcept { return depth_.get(); }

private:
    static inline DirectXCommon *sDirectXCommon_ = nullptr;

    ShadowMapBuffer() = default;

    /// @brief リソース初期化
    bool Initialize(std::uint32_t width, std::uint32_t height, DXGI_FORMAT depthFormat, DXGI_FORMAT srvFormat);

    /// @brief 破棄
    void Destroy();

    /// @brief コマンド記録開始
    ID3D12GraphicsCommandList *BeginRecord();

    /// @brief コマンド記録終了
    bool EndRecord();

    /// @brief TextureManager への登録（名前が空の場合は自動生成）
    void RegisterToTextureManager(const std::string &name);
    /// @brief TextureManager からの登録解除
    void UnregisterFromTextureManager();

    std::string name_;
    std::uint32_t textureHandle_ = 0;

    std::uint32_t width_ = 0;
    std::uint32_t height_ = 0;

    DXGI_FORMAT depthFormat_ = DXGI_FORMAT_UNKNOWN;
    DXGI_FORMAT srvFormat_ = DXGI_FORMAT_UNKNOWN;

    std::unique_ptr<DepthStencilResource> depth_;

    int commandSlotIndex_ = -1;
    DX12Commands *dx12Commands_ = nullptr;
};

} // namespace KashipanEngine
