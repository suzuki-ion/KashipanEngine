#pragma once
#include <cstdint>
#include <string>
#include <d3d12.h>

namespace KashipanEngine {

/// @brief 描画先の種類
enum class RenderTargetKind {
    Window,
    ScreenBuffer,
    ShadowMapBuffer,
};

/// @brief 描画先インターフェースクラス
class IRenderTarget {
public:
    virtual ~IRenderTarget() = default;

    virtual RenderTargetKind GetRenderTargetKind() const noexcept = 0;
    virtual std::string GetRenderTargetName() const = 0;
    virtual std::uint32_t GetRenderTargetWidth() const noexcept = 0;
    virtual std::uint32_t GetRenderTargetHeight() const noexcept = 0;
    virtual bool IsRenderTargetAvailable() const noexcept = 0;

    virtual void BeginRender() = 0;
    virtual void EndRender() = 0;
    virtual ID3D12CommandList *GetCommandList() const = 0;
};

} // namespace KashipanEngine
