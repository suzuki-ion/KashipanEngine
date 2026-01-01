#include "GraphicsEngine.h"
#include "Core/DirectXCommon.h"
#include "Core/Window.h"
#include "Graphics/PipelineManager.h"
#include "Graphics/Renderer.h"
#include "EngineSettings.h"
#include "Graphics/Resources.h"

#include "Objects/GameObjects/2D/Triangle2D.h"
#include "Objects/GameObjects/2D/Ellipse.h"
#include "Objects/GameObjects/2D/Rect.h"
#include "Objects/GameObjects/2D/Sprite.h"
#include "Objects/SystemObjects/Camera2D.h"
#include "Objects/Components/2D/Material2D.h"
#include "Objects/Components/2D/Transform2D.h"

#include "Objects/GameObjects/3D/Triangle3D.h"
#include "Objects/GameObjects/3D/Sphere.h"
#include "Objects/GameObjects/3D/Box.h"
#include "Objects/GameObjects/3D/Model.h"
#include "Objects/SystemObjects/Camera3D.h"
#include "Objects/SystemObjects/DirectionalLight.h"
#include "Objects/Components/3D/Material3D.h"
#include "Objects/Components/3D/Transform3D.h"

namespace KashipanEngine {

GraphicsEngine::GraphicsEngine(Passkey<GameEngine>, DirectXCommon *directXCommon)
    : directXCommon_(directXCommon) {
    auto *device = directXCommon_->GetDevice(Passkey<GraphicsEngine>{});
    auto &settings = GetEngineSettings().rendering;
    pipelineManager_ = std::make_unique<PipelineManager>(Passkey<GraphicsEngine>{},
        device, settings.pipelineSettingsPath);
    Window::SetPipelineManager(Passkey<GraphicsEngine>{}, pipelineManager_.get());
    renderer_ = std::make_unique<Renderer>(Passkey<GraphicsEngine>{}, 1024, directXCommon_, pipelineManager_.get());
    Window::SetRenderer(Passkey<GraphicsEngine>{}, renderer_.get());
}

GraphicsEngine::~GraphicsEngine() {
    Window::SetPipelineManager(Passkey<GraphicsEngine>{}, nullptr);
    Window::SetRenderer(Passkey<GraphicsEngine>{}, nullptr);
}

void GraphicsEngine::RenderFrame(Passkey<GameEngine>) {
    renderer_->RenderFrame({});
}

} // namespace KashipanEngine
