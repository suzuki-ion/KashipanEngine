#pragma once
#include <cstdint>
#include <string>

namespace KashipanEngine {

enum class RenderTargetKind {
    Window,
    ScreenBuffer,
    ShadowMapBuffer,
};

class IRenderTarget {
public:
    virtual ~IRenderTarget() = default;

    virtual RenderTargetKind GetRenderTargetKind() const noexcept = 0;
    virtual std::string GetRenderTargetName() const = 0;
    virtual std::uint32_t GetRenderTargetWidth() const noexcept = 0;
    virtual std::uint32_t GetRenderTargetHeight() const noexcept = 0;
    virtual bool IsRenderTargetAvailable() const noexcept = 0;
};

} // namespace KashipanEngine
