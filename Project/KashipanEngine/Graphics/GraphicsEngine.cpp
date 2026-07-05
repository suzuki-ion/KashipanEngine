#include "GraphicsEngine.h"
#include "Core/DirectXCommon.h"
#include "Graphics/PipelineManager.h"
#include "Graphics/Renderer/Renderer.h"
#include "EngineSettings.h"

namespace KashipanEngine {

GraphicsEngine::GraphicsEngine(Passkey<GameEngine>, DirectXCommon *directXCommon)
    : directXCommon_(directXCommon) {
    auto *device = directXCommon_->GetDevice(Passkey<GraphicsEngine>{});
    auto &settings = GetEngineSettings().rendering;
    pipelineManager_ = std::make_unique<PipelineManager>(Passkey<GraphicsEngine>{},
        device, settings.pipelineSettingsPath);
    renderer_ = std::make_unique<Renderer>(Passkey<GraphicsEngine>{}, directXCommon_, pipelineManager_.get());
}

GraphicsEngine::~GraphicsEngine() = default;

void GraphicsEngine::RenderFrame(Passkey<GameEngine>, SceneContext *sceneContext) {
    renderer_->RenderFrame({}, sceneContext);
}

void GraphicsEngine::ReleaseRendererResources(Passkey<GameEngine>) {
    renderer_->ReleaseAllResources({});
}

} // namespace KashipanEngine
