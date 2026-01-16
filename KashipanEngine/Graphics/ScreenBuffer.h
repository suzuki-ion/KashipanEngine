#pragma once
#include <cstdint>
#include <memory>
#include <vector>
#include <string>
#include <optional>
#include <unordered_map>

#include "Core/DirectX/DX12Commands.h"
#include "Utilities/Passkeys.h"
#include "PostEffectComponents/IPostEffectComponent.h"
#include "Graphics/Renderer.h"
#include "Graphics/Resources/RenderTargetResource.h"
#include "Graphics/Resources/DepthStencilResource.h"
#include "Graphics/Resources/ShaderResourceResource.h"
#include "Graphics/IShaderTexture.h"

namespace KashipanEngine {

class GameEngine;
class DirectXCommon;
class Window;

/// @brief オフスクリーンレンダリング用スクリーンバッファ
class ScreenBuffer final : public IShaderTexture {
public:
    /// @brief GameEngine から DirectXCommon を設定
    static void SetDirectXCommon(Passkey<GameEngine>, DirectXCommon *dx) { sDirectXCommon_ = dx; }

    /// @brief GameEngine から Renderer を設定（persistent pass 登録用）
    static void SetRenderer(Passkey<GameEngine>, Renderer* renderer) { sRenderer = renderer; }

    /// @brief ScreenBuffer 生成
    static ScreenBuffer *Create(std::uint32_t width, std::uint32_t height,
        DXGI_FORMAT colorFormat = DXGI_FORMAT_B8G8R8A8_UNORM,
        DXGI_FORMAT depthFormat = DXGI_FORMAT_D24_UNORM_S8_UINT);

    /// @brief 全 ScreenBuffer 破棄
    static void AllDestroy(Passkey<GameEngine>);

    /// @brief ScreenBuffer インスタンス数取得
    static size_t GetBufferCount();

    /// @brief ポインタから存在確認
    static bool IsExist(ScreenBuffer *buffer);

    /// @brief Renderer 用: 全 ScreenBuffer のコマンド記録開始
    static void AllBeginRecord(Passkey<Renderer>);

    /// @brief Renderer 用: 全 ScreenBuffer のコマンド記録終了
    static std::vector<ID3D12CommandList *> AllEndRecord(Passkey<Renderer>);

    /// @brief Renderer 用: 全 ScreenBuffer のコマンドを Close する
    static void AllCloseRecord(Passkey<Renderer>);

    /// @brief Renderer 用: 指定バッファを discard としてマーク（AllEndRecord で反映）
    static void MarkDiscard(Passkey<Renderer>, ScreenBuffer *buffer);

    /// @brief アプリ側から破棄要求（実体の破棄は CommitDestroy で行う）
    static void DestroyNotify(ScreenBuffer *buffer);

    /// @brief 破棄要求済み ScreenBuffer をフレーム終端で実際に破棄する
    static void CommitDestroy(Passkey<GameEngine>);

    /// @brief 指定バッファが破棄要求済みか
    static bool IsPendingDestroy(ScreenBuffer *buffer);

    ~ScreenBuffer();

    ScreenBuffer(const ScreenBuffer &) = delete;
    ScreenBuffer &operator=(const ScreenBuffer &) = delete;
    ScreenBuffer(ScreenBuffer &&) = delete;
    ScreenBuffer &operator=(ScreenBuffer &&) = delete;

    std::uint32_t GetWidth() const noexcept override { return width_; }
    std::uint32_t GetHeight() const noexcept override { return height_; }

    D3D12_GPU_DESCRIPTOR_HANDLE GetSrvHandle() const noexcept override;

    /// @brief Depth(読み取り面)をテクスチャ(SRV)として参照するためのハンドル（未作成なら空）
    D3D12_GPU_DESCRIPTOR_HANDLE GetDepthSrvHandle() const noexcept;

    RenderTargetResource *GetRenderTarget() const noexcept { return renderTargets_[GetRtvReadIndex()].get(); }
    DepthStencilResource *GetDepthStencil() const noexcept { return depthStencils_[GetDsvReadIndex()].get(); }
    ShaderResourceResource *GetShaderResource() const noexcept { return shaderResources_[GetRtvReadIndex()].get(); }

    /// @brief ポストエフェクトコンポーネント登録
    bool RegisterPostEffectComponent(std::unique_ptr<IPostEffectComponent> component);

    /// @brief 登録済みポストエフェクトコンポーネントを全て取得
    const std::vector<std::unique_ptr<IPostEffectComponent>> &GetPostEffectComponents() const { return postEffectComponents_; }

    /// @brief Renderer用: 登録済みポストエフェクトをレンダーパスとして列挙
    std::vector<PostEffectPass> BuildPostEffectPasses(Passkey<Renderer>) const;

    /// @brief Renderer が定数/インスタンスバッファキャッシュ等で使う擬似キー
    HWND GetCacheKey() const noexcept { return reinterpret_cast<HWND>(const_cast<ScreenBuffer *>(this)); }

    void AttachToRenderer(const std::string &passName = "");
    void DetachFromRenderer();

    /// @brief Renderer 用: 記録中コマンドリスト取得（AllBeginRecord 後）
    ID3D12GraphicsCommandList *GetRecordedCommandList(Passkey<Renderer>) const noexcept { return dx12Commands_->GetCommandList(); }

    /// @brief Renderer 用: 現在フレームで記録開始されているか
    bool IsRecording(Passkey<Renderer>) const noexcept;

    /// @brief Renderer 用: この ScreenBuffer の記録を開始（深度を上書きしない用途向けに制御可能）
    /// @param disableDepthWrite true の場合、Depth への書き込み/クリアを行わず Color のみをターゲットにする
    ID3D12GraphicsCommandList *BeginRecord(Passkey<Renderer>, bool disableDepthWrite);

    /// @brief Renderer 用: この ScreenBuffer の記録を終了
    bool EndRecord(Passkey<Renderer>, bool discard = false);

    /// @brief Renderer 用: DX12Commands 取得（ポストエフェクト等で使用）
    DX12Commands *GetDX12Commands(Passkey<Renderer>) const noexcept { return dx12Commands_; }

#if defined(USE_IMGUI)
    /// @brief デバッグ用: 生成済み ScreenBuffer の内容を表示する ImGui ウィンドウを描画
    static void ShowImGuiScreenBuffersWindow();
#endif

private:
    std::optional<ScreenBufferPass> CreateScreenPass(const std::string &passName);

    static inline DirectXCommon *sDirectXCommon_ = nullptr;
    static inline Renderer* sRenderer = nullptr;
    static constexpr size_t kBufferCount_ = 2;

    ScreenBuffer() = default;

    size_t GetRtvWriteIndex() const noexcept { return rtvWriteIndex_; }
    size_t GetRtvReadIndex() const noexcept { return (rtvWriteIndex_ + 1) % kBufferCount_; }
    size_t GetDsvWriteIndex() const noexcept { return dsvWriteIndex_; }
    size_t GetDsvReadIndex() const noexcept { return (dsvWriteIndex_ + 1) % kBufferCount_; }

    void AdvanceFrameBufferIndex(bool updateRtv, bool updateDsv) noexcept {
        if (updateRtv) {
            rtvWriteIndex_ = (rtvWriteIndex_ + 1) % kBufferCount_;
        }
        if (updateDsv) {
            dsvWriteIndex_ = (dsvWriteIndex_ + 1) % kBufferCount_;
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

    std::uint32_t width_ = 0;
    std::uint32_t height_ = 0;

    DXGI_FORMAT colorFormat_ = DXGI_FORMAT_UNKNOWN;
    DXGI_FORMAT depthFormat_ = DXGI_FORMAT_UNKNOWN;

    size_t rtvWriteIndex_ = 0;
    size_t dsvWriteIndex_ = 0;

    bool lastBeginDisableDepthWrite_ = false;

    std::unique_ptr<RenderTargetResource> renderTargets_[kBufferCount_];
    std::unique_ptr<DepthStencilResource> depthStencils_[kBufferCount_];
    std::unique_ptr<ShaderResourceResource> shaderResources_[kBufferCount_];

    std::vector<std::unique_ptr<IPostEffectComponent>> postEffectComponents_;

    int commandSlotIndex_ = -1;
    DX12Commands *dx12Commands_ = nullptr;

    Renderer::PersistentScreenPassHandle persistentScreenPassHandle_;
};

} // namespace KashipanEngine
