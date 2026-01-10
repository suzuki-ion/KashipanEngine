#pragma once
#include <cstdint>
#include <memory>
#include <unordered_map>

#include "Utilities/Passkeys.h"
#include "Graphics/Resources/DepthStencilResource.h"
#include "Graphics/IShaderTexture.h"

namespace KashipanEngine {

class GameEngine;
class DirectXCommon;
class Renderer;

/// @brief シャドウマップ生成用（深度のみ）オフスクリーンバッファ
class ShadowMapBuffer final : public IShaderTexture {
public:
    static void SetDirectXCommon(Passkey<GameEngine>, DirectXCommon* dx) { sDirectXCommon_ = dx; }

    /// @brief ShadowMapBuffer 生成
    /// @param width シャドウマップ解像度
    /// @param height シャドウマップ解像度
    /// @param depthFormat DSV 用フォーマット（例: DXGI_FORMAT_D32_FLOAT）
    /// @param srvFormat SRV 用フォーマット（例: DXGI_FORMAT_R32_FLOAT）
    static ShadowMapBuffer* Create(std::uint32_t width, std::uint32_t height,
        DXGI_FORMAT depthFormat = DXGI_FORMAT_D32_FLOAT,
        DXGI_FORMAT srvFormat = DXGI_FORMAT_R32_FLOAT);

    static void AllDestroy(Passkey<GameEngine>);
    static size_t GetBufferCount();
    static bool IsExist(ShadowMapBuffer* buffer);

    /// @brief Renderer 用: 全 ShadowMapBuffer のコマンド記録開始
    static void AllBeginRecord(Passkey<Renderer>);

    /// @brief Renderer 用: 全 ShadowMapBuffer のコマンド記録終了
    static std::vector<ID3D12CommandList*> AllEndRecord(Passkey<Renderer>);

    ~ShadowMapBuffer();

    ShadowMapBuffer(const ShadowMapBuffer&) = delete;
    ShadowMapBuffer& operator=(const ShadowMapBuffer&) = delete;
    ShadowMapBuffer(ShadowMapBuffer&&) = delete;
    ShadowMapBuffer& operator=(ShadowMapBuffer&&) = delete;

    std::uint32_t GetWidth() const noexcept override { return width_; }
    std::uint32_t GetHeight() const noexcept override { return height_; }

    /// @brief シャドウマップ SRV
    D3D12_GPU_DESCRIPTOR_HANDLE GetSrvHandle() const noexcept override;

    DepthStencilResource* GetDepth() const noexcept { return depth_.get(); }

    /// @brief Renderer 用: 記録中コマンドリスト取得（AllBeginRecord 後）
    ID3D12GraphicsCommandList* GetRecordedCommandList(Passkey<Renderer>) const noexcept { return commandList_; }

    /// @brief Renderer 用: 現在フレームで記録開始されているか
    bool IsRecording(Passkey<Renderer>) const noexcept;

    /// @brief アプリ側から破棄要求（実体の破棄は CommitDestroy で行う）
    static void DestroyNotify(ShadowMapBuffer* buffer);

    /// @brief 破棄要求済み ShadowMapBuffer をフレーム終端で実際に破棄する
    static void CommitDestroy(Passkey<GameEngine>);

    /// @brief 指定バッファが破棄要求済みか
    static bool IsPendingDestroy(ShadowMapBuffer* buffer);

#if defined(USE_IMGUI)
    /// @brief デバッグ用: 生成済み ShadowMapBuffer の内容を表示する ImGui ウィンドウを描画
    static void ShowImGuiShadowMapBuffersWindow();
#endif

private:
    static inline DirectXCommon* sDirectXCommon_ = nullptr;
    static std::unordered_map<ShadowMapBuffer*, std::unique_ptr<ShadowMapBuffer>> sBufferMap_;

    ShadowMapBuffer() = default;

    bool Initialize(std::uint32_t width, std::uint32_t height, DXGI_FORMAT depthFormat, DXGI_FORMAT srvFormat);
    void Destroy();

    ID3D12GraphicsCommandList* BeginRecord();
    bool EndRecord(bool discard = false);

    std::uint32_t width_ = 0;
    std::uint32_t height_ = 0;

    DXGI_FORMAT depthFormat_ = DXGI_FORMAT_UNKNOWN;
    DXGI_FORMAT srvFormat_ = DXGI_FORMAT_UNKNOWN;

    std::unique_ptr<DepthStencilResource> depth_;

    int commandSlotIndex_ = -1;
    ID3D12CommandAllocator* commandAllocator_ = nullptr;
    ID3D12GraphicsCommandList* commandList_ = nullptr;
};

} // namespace KashipanEngine
