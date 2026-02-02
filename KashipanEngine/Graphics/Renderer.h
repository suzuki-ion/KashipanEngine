#pragma once
#include <d3d12.h>
#include "Core/Window.h"
#include "Graphics/ScreenBuffer.h"
#include "Graphics/ShadowMapBuffer.h"
#include "Utilities/EntityComponentSystem.h"

namespace KashipanEngine {

/// @brief 描画パス設定用コンポーネント
struct RenderPassComponent {
    struct RenderPass {
        Window *renderTargetWindow = nullptr;
        ScreenBuffer *renderTargetBuffer = nullptr;
        ShadowMapBuffer *shadowMapBuffer = nullptr;
    };
    std::vector<RenderPass> renderPass;
};

} // namespace KashipanEngine