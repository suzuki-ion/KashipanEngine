#pragma once

#include <cstddef>
#include <cstdint>
#include <string>
#include <vector>

#include "Assets/MeshAssets.h"
#include "Graphics/IShaderTexture.h"

namespace KashipanEngine {

class Window;
class ScreenBuffer;
class ShadowMapBuffer;

enum class RenderTargetType {
    Window,
    ScreenBuffer,
    ShadowMapBuffer,
};

struct RenderTarget {
    RenderTargetType type = RenderTargetType::Window;
    void *target = nullptr;

    bool IsValid() const { return target != nullptr; }

    Window *AsWindow() const {
        return type == RenderTargetType::Window ? static_cast<Window *>(target) : nullptr;
    }

    ScreenBuffer *AsScreenBuffer() const {
        return type == RenderTargetType::ScreenBuffer ? static_cast<ScreenBuffer *>(target) : nullptr;
    }

    ShadowMapBuffer *AsShadowMapBuffer() const {
        return type == RenderTargetType::ShadowMapBuffer ? static_cast<ShadowMapBuffer *>(target) : nullptr;
    }
};

struct RenderPassBindingInfo {
    std::string shaderNameKey;
    std::vector<std::byte> data;
};

struct RenderPassInfo {
    RenderTarget target;
    std::string pipelineName;
    std::string passName;
    std::int32_t priority = 0;
    MeshHandle mesh;
    IShaderTexture *texture = nullptr;
    std::vector<RenderPassBindingInfo> bindings;
};

struct RenderPassesComponent {
    std::vector<RenderPassInfo> passes;
};

} // namespace KashipanEngine
