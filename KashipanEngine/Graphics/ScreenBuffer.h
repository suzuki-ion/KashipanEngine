#pragma once
#include <cstdint>
#include <memory>
#include <vector>
#include <string>
#include <optional>
#include <unordered_map>

#include "Utilities/Passkeys.h"
#include "Graphics/IPostEffectComponent.h"
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

    /// @brief ScreenBuffer 生成
    static ScreenBuffer *Create(Window *targetWindow, std::uint32_t width, std::uint32_t height,
        RenderDimension dimension,
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

    /// @brief Renderer 用: 指定バッファを discard としてマーク（AllEndRecord で反映）
    static void MarkDiscard(Passkey<Renderer>, ScreenBuffer *buffer);

    ~ScreenBuffer();

    ScreenBuffer(const ScreenBuffer &) = delete;
    ScreenBuffer &operator=(const ScreenBuffer &) = delete;
    ScreenBuffer(ScreenBuffer &&) = delete;
    ScreenBuffer &operator=(ScreenBuffer &&) = delete;

    std::uint32_t GetWidth() const noexcept override { return width_; }
    std::uint32_t GetHeight() const noexcept override { return height_; }

    D3D12_GPU_DESCRIPTOR_HANDLE GetSrvHandle() const noexcept override;

    RenderTargetResource *GetRenderTarget() const noexcept { return renderTargets_[GetReadIndex()].get(); }
    DepthStencilResource *GetDepthStencil() const noexcept { return depthStencils_[GetWriteIndex()].get(); }
    ShaderResourceResource *GetShaderResource() const noexcept { return shaderResources_[GetReadIndex()].get(); }

    /// @brief ポストエフェクトコンポーネント登録
    bool RegisterPostEffectComponent(std::unique_ptr<IPostEffectComponent> component);

    /// @brief 登録済みポストエフェクトコンポーネントを全て取得
    const std::vector<std::unique_ptr<IPostEffectComponent>> &GetPostEffectComponents() const { return postEffectComponents_; }

    /// @brief 描画実行（RenderPass から呼ばれる想定）
    bool RenderBatched(ShaderVariableBinder &binder, std::uint32_t instanceCount);

    /// @brief Renderer が定数/インスタンスバッファキャッシュ等で使う擬似キー
    HWND GetCacheKey() const noexcept { return reinterpret_cast<HWND>(const_cast<ScreenBuffer *>(this)); }

    void AttachToRenderer(const std::string &pipelineName, const std::string &passName);
    void DetachFromRenderer();

    /// @brief Renderer 用: 記録中コマンドリスト取得（AllBeginRecord 後）
    ID3D12GraphicsCommandList *GetRecordedCommandList(Passkey<Renderer>) const noexcept { return commandList_; }

    /// @brief Renderer 用: 現在フレームで記録開始されているか
    bool IsRecording(Passkey<Renderer>) const noexcept;

#if defined(USE_IMGUI)
    /// @brief デバッグ用: 生成済み ScreenBuffer の内容を表示する ImGui ウィンドウを描画
    static void ShowImGuiScreenBuffersWindow();
#endif

private:
    std::optional<ScreenBufferPass> CreateScreenPass(const std::string &pipelineName, const std::string &passName);

    static inline DirectXCommon *sDirectXCommon_ = nullptr;

    static std::unordered_map<ScreenBuffer *, std::unique_ptr<ScreenBuffer>> sBufferMap_;

    ScreenBuffer() = default;

    static constexpr size_t kBufferCount_ = 2;

    size_t GetWriteIndex() const noexcept { return writeIndex_; }
    size_t GetReadIndex() const noexcept { return (writeIndex_ + 1) % kBufferCount_; }
    void AdvanceFrameBufferIndex() noexcept { writeIndex_ = (writeIndex_ + 1) % kBufferCount_; }

    /// @brief リソース初期化
    bool Initialize(Window *targetWindow, std::uint32_t width, std::uint32_t height,
        RenderDimension dimension,
        DXGI_FORMAT colorFormat,
        DXGI_FORMAT depthFormat);

    /// @brief 破棄
    void Destroy();

    /// @brief コマンド記録開始（Renderer のオフスクリーン描画用）
    ID3D12GraphicsCommandList *BeginRecord();

    /// @brief コマンド記録終了
    bool EndRecord(bool discard = false);

    Window *targetWindow_ = nullptr;
    RenderDimension dimension_ = RenderDimension::D2;

    std::uint32_t width_ = 0;
    std::uint32_t height_ = 0;

    DXGI_FORMAT colorFormat_ = DXGI_FORMAT_UNKNOWN;
    DXGI_FORMAT depthFormat_ = DXGI_FORMAT_UNKNOWN;

    size_t writeIndex_ = 0;

    std::unique_ptr<RenderTargetResource> renderTargets_[kBufferCount_];
    std::unique_ptr<DepthStencilResource> depthStencils_[kBufferCount_];
    std::unique_ptr<ShaderResourceResource> shaderResources_[kBufferCount_];

    std::vector<std::unique_ptr<IPostEffectComponent>> postEffectComponents_;

    int commandSlotIndex_ = -1;
    ID3D12CommandAllocator *commandAllocator_ = nullptr;
    ID3D12GraphicsCommandList *commandList_ = nullptr;

    Renderer::PersistentScreenPassHandle persistentScreenPassHandle_;
};

} // namespace KashipanEngine
