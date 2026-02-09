#include "SceneDefaultVariables.h"
#include "Scene/SceneContext.h"

namespace KashipanEngine {

void SceneDefaultVariables::Initialize() {
    mainWindow_ = Window::GetWindow("Main Window");

    screenBuffer3D_ = ScreenBuffer::Create(1920, 1080);
    screenBuffer2D_ = ScreenBuffer::Create(1920, 1080);
    shadowMapBuffer_ = ShadowMapBuffer::Create(2048, 2048);
}

void SceneDefaultVariables::Finalize() {
    ScreenBuffer::DestroyNotify(screenBuffer3D_);
    ScreenBuffer::DestroyNotify(screenBuffer2D_);
    ShadowMapBuffer::DestroyNotify(shadowMapBuffer_);
}

void SceneDefaultVariables::Update() {
}

void SceneDefaultVariables::SetSceneComponents(std::function<bool(std::unique_ptr<ISceneComponent>)> registerFunc) {
    // Colliderコンポーネント
    {
        auto comp = std::make_unique<ColliderComponent>();
        registerFunc(std::move(comp));
    }
}

} // namespace KashipanEngine