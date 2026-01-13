#pragma once
#include <memory>
#include <vector>
#include "Graphics/PostEffectComponents/IPostEffectComponent.h"
#include "Graphics/ScreenBuffer.h"

namespace KashipanEngine {

/// @brief ScreenBuffer の Color(SRV) をそのまま ScreenBuffer の RT に描画する最小サンプル
/// @details 実際のシェーダやパイプラインはアセット側で用意する前提。
class CopyColorToScreenBufferEffect final : public IPostEffectComponent {
public:
    CopyColorToScreenBufferEffect(std::string pipelineName)
        : IPostEffectComponent("CopyColorToScreenBufferEffect", 1), pipelineName_(std::move(pipelineName)) {}

    std::unique_ptr<IPostEffectComponent> Clone() const override {
        return std::make_unique<CopyColorToScreenBufferEffect>(pipelineName_);
    }

    std::vector<PostEffectPass> BuildPostEffectPasses() const override {
        PostEffectPass p;
        p.pipelineName = pipelineName_;
        p.passName = "CopyColorToScreenBufferEffect";
        p.batchKey = 0;

        p.batchedRenderFunction = [this](ShaderVariableBinder &binder, std::uint32_t /*instanceCount*/) -> bool {
            auto *owner = GetOwnerBuffer();
            if (!owner) return false;

            if (binder.GetNameMap().Contains("Pixel:gTexture")) {
                return binder.Bind("Pixel:gTexture", owner->GetSrvHandle());
            }
            return true;
        };

        p.renderCommandFunction = [this](PipelineBinder &/*pb*/) -> std::optional<RenderCommand> {
            return MakeDrawCommand(3);
        };

        return { std::move(p) };
    }

private:
    std::string pipelineName_;
};

} // namespace KashipanEngine
