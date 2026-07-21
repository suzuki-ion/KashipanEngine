#pragma once
#include <cstdint>
#include <memory>
#include <vector>
#include <string>
#include <optional>
#include <unordered_map>

#include "Core/DirectX/DX12Commands.h"
#include "Graphics/Resources/RenderTargetResource.h"
#include "Graphics/Resources/DepthStencilResource.h"
#include "Graphics/Resources/ShaderResourceResource.h"
#include "Graphics/IShaderTexture.h"
#include "Graphics/IRenderTarget.h"

namespace KashipanEngine {

class GameEngine;
class DirectXCommon;
class Window;
class Renderer;

/// @brief オフスクリーンレンダリング用スクリーンバッファ
class ScreenBuffer final : public IShaderTexture, public IRenderTarget {
public:
    /// @brief GameEngine から DirectXCommon を設定
    static void SetDirectXCommon(Passkey<GameEngine>, DirectXCommon *dx) { sDirectXCommon_ = dx; }
    /// @brief 全 ScreenBuffer 破棄
    static void AllDestroy(Passkey<GameEngine>);
    /// @brief 破棄要求済み ScreenBuffer をフレーム終端で実際に破棄する
    static void CommitDestroy(Passkey<GameEngine>);

    /// @brief ScreenBuffer 生成
    /// @param name 管理用の名前（空の場合は自動生成。TextureManagerにこの名前で登録される）
    static ScreenBuffer *Create(std::uint32_t width, std::uint32_t height,
        const std::string &name = "",
        DXGI_FORMAT colorFormat = DXGI_FORMAT_B8G8R8A8_UNORM,
        DXGI_FORMAT depthFormat = DXGI_FORMAT_D24_UNORM_S8_UINT);
    /// @brief ポインタから存在確認
    static bool IsExist(ScreenBuffer *buffer);

    ScreenBuffer(const ScreenBuffer &) = delete;
    ScreenBuffer &operator=(const ScreenBuffer &) = delete;
    ScreenBuffer(ScreenBuffer &&) = delete;
    ScreenBuffer &operator=(ScreenBuffer &&) = delete;

    ~ScreenBuffer();

    /// @brief 指定バッファが破棄要求済みか
    bool IsPendingDestroy() const;
    /// @brief アプリ側から破棄要求（実体の破棄は CommitDestroy で行う）
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

    RenderTargetKind GetRenderTargetKind() const noexcept override { return RenderTargetKind::ScreenBuffer; }
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
    ID3D12GraphicsCommandList *GetCommandList() const override { return dx12Commands_->GetCommandList(); }

    /// @brief ポストエフェクト用のパス切り替え処理
    /// @details 現在の書き込み面をSRVとして参照可能な状態にしてバッファを進め、
    ///          次の書き込み面への描画（深度書き込みなし）を開始する。
    ///          BeginDraw と EndDraw の間でのみ呼び出すこと。
    void NextPass();

    /// @brief 現在の書き込み面を再度レンダーターゲットとして設定する
    /// @details ポストエフェクトが内部レンダーターゲットへ描画した後、
    ///          描画先をこのバッファへ戻すために呼ぶ（クリアは行わない）。
    void RebindWriteTarget();

    /// @brief このフレームの描画（ポストエフェクト適用含む）が完了した時点のSRVハンドルを記録する
    /// @details ScreenBufferObjectのビューア用ImGuiウィンドウは、Renderer::RenderFrame より前の
    ///          タイミング（GameLoopUpdate側）で構築されるため、その場で GetSrvHandle() を呼ぶと
    ///          今フレームのポストエフェクト適用が終わる前の読み取り面インデックスを参照してしまい、
    ///          適用したポストエフェクトの数（NextPass呼び出し回数の偶奇）によっては
    ///          エフェクト適用途中の中間結果を表示してしまう。
    ///          Renderer側がこのフレームの描画完了直後（EndDraw後）に確定した正しいSRVハンドルを
    ///          ここへ記録し、ビューアはこちらを参照することで常に「完成した最新フレーム」を表示する。
    void SetPreviewSrvHandle(Passkey<Renderer>, D3D12_GPU_DESCRIPTOR_HANDLE handle) noexcept { previewSrvHandle_ = handle; }
    /// @brief 直近で描画が完了した時点のSRVハンドルを取得（ビューア表示用）
    D3D12_GPU_DESCRIPTOR_HANDLE GetPreviewSrvHandle() const noexcept { return previewSrvHandle_; }

    //==================================================
    // ハンドルとリソース取得
    //==================================================

    /// @brief Depth(読み取り面)をテクスチャ(SRV)として参照するためのハンドル（未作成なら空）
    D3D12_GPU_DESCRIPTOR_HANDLE GetDepthSrvHandle() const noexcept;

    RenderTargetResource *GetRenderTarget() const noexcept { return renderTargets_[GetRtvReadIndex()].get(); }
    DepthStencilResource *GetDepthStencil() const noexcept { return depthStencils_[GetDsvReadIndex()].get(); }
    ShaderResourceResource *GetShaderResource() const noexcept { return shaderResources_[GetRtvReadIndex()].get(); }

    /// @brief 深度書き込みの有効/無効を設定（ポストエフェクト用や2D描画用。次のBeginDrawから反映される）
    /// @param enable true の場合、深度書き込みを有効にする。false の場合、深度書き込みを無効にする。
    void SetDepthWriteEnabled(bool enable) noexcept { isDepthWriteEnabled_ = enable; }

    /// @brief バッファサイズの変更を通知する（GPUリソースの再生成は即時には行わない）
    /// @details 呼び出した時点では新しいサイズを記憶するだけで、実際のGPUリソース再生成は
    ///          BeginDraw（内部的にはBeginRecord）でそのリソースが実際に使用される際、
    ///          そのリソース単体のみを対象に行われる（ダブルバッファの全リソースを一括で
    ///          作り直すと、まだ使われていない方のバッファまで空の状態になり、その内容が
    ///          そのまま画面に一瞬表示されてちらつく問題があったため）。
    ///          TextureManagerへの登録名・ハンドルはそのまま維持される（サイズはこのオブジェクトから
    ///          毎回取得されるため再登録は不要）。BeginDraw/EndDraw の外側から呼ぶこと。
    /// @return 成功した場合はtrue、失敗した場合（未初期化・サイズが0等）はfalseを返す
    bool Resize(std::uint32_t width, std::uint32_t height);

private:
    static inline DirectXCommon *sDirectXCommon_ = nullptr;
    static constexpr size_t kBufferCount = 2;

    ScreenBuffer() = default;

    size_t GetRtvWriteIndex() const noexcept { return rtvWriteIndex_; }
    size_t GetRtvReadIndex() const noexcept { return (rtvWriteIndex_ + 1) % kBufferCount; }
    size_t GetDsvWriteIndex() const noexcept { return dsvWriteIndex_; }
    size_t GetDsvReadIndex() const noexcept { return (dsvWriteIndex_ + 1) % kBufferCount; }

    void AdvanceFrameBufferIndex(bool updateRtv, bool updateDsv) noexcept {
        if (updateRtv) {
            rtvWriteIndex_ = (rtvWriteIndex_ + 1) % kBufferCount;
        }
        if (updateDsv) {
            dsvWriteIndex_ = (dsvWriteIndex_ + 1) % kBufferCount;
        }
    }

    /// @brief リソース初期化
    bool Initialize(std::uint32_t width, std::uint32_t height,
        DXGI_FORMAT colorFormat, DXGI_FORMAT depthFormat);

    /// @brief 破棄
    void Destroy();

    /// @brief コマンド記録開始
    ID3D12GraphicsCommandList *BeginRecord(bool disableDepthWrite);

    /// @brief コマンド記録終了
    bool EndRecord(bool discard = false);

    /// @brief 指定インデックスのRenderTarget/ShaderResourceが現在の width_/height_ と異なるサイズの
    ///        場合のみ、そのインデックスのリソースだけを新しいサイズで作り直す
    /// @details RenderTarget用とDepthStencil用でインデックスの進み方が異なる場合があるため
    ///          （深度書き込み無効時はDSVインデックスが進まない）、それぞれ独立して管理する
    void EnsureRenderTargetSize(size_t index, ID3D12GraphicsCommandList *cmd);
    /// @brief 指定インデックスのDepthStencilが現在の width_/height_ と異なるサイズの場合のみ、
    ///        そのインデックスのリソースだけを新しいサイズで作り直す
    void EnsureDepthStencilSize(size_t index, ID3D12GraphicsCommandList *cmd);

    /// @brief TextureManager への登録（名前が空の場合は自動生成）
    void RegisterToTextureManager(const std::string &name);
    /// @brief TextureManager からの登録解除
    void UnregisterFromTextureManager();

    std::string name_;
    std::uint32_t textureHandle_ = 0;
    /// @brief 直近で描画完了が確定した時点のSRVハンドル（ビューア用。SetPreviewSrvHandle参照）
    D3D12_GPU_DESCRIPTOR_HANDLE previewSrvHandle_{};

    std::uint32_t width_ = 0;
    std::uint32_t height_ = 0;

    DXGI_FORMAT colorFormat_ = DXGI_FORMAT_UNKNOWN;
    DXGI_FORMAT depthFormat_ = DXGI_FORMAT_UNKNOWN;

    size_t rtvWriteIndex_ = 0;
    size_t dsvWriteIndex_ = 0;

    bool isDepthWriteEnabled_ = true;
    bool isLastBeginDisableDepthWrite_ = false;
    bool isFirstBeginRecord_ = true;

    std::unique_ptr<RenderTargetResource> renderTargets_[kBufferCount];
    std::unique_ptr<DepthStencilResource> depthStencils_[kBufferCount];
    std::unique_ptr<ShaderResourceResource> shaderResources_[kBufferCount];

    // 各インデックスのRenderTarget/DepthStencilが実際にGPU上で確保されているサイズ
    // （width_/height_とは異なる場合がある。Resize()は即時にはGPUリソースを作り直さず、
    // このサイズとの差分をEnsureRenderTargetSize/EnsureDepthStencilSizeで検知して
    // 使用時に1バッファずつ作り直す。RTVとDSVでインデックスの進み方が異なることがあるため別々に持つ）
    std::uint32_t rtBufferWidth_[kBufferCount]{};
    std::uint32_t rtBufferHeight_[kBufferCount]{};
    std::uint32_t dsBufferWidth_[kBufferCount]{};
    std::uint32_t dsBufferHeight_[kBufferCount]{};

    int commandSlotIndex_ = -1;
    DX12Commands *dx12Commands_ = nullptr;
};

} // namespace KashipanEngine
