#pragma once
#include <KashipanEngine.h>

namespace KashipanEngine {

class ResultScene final : public SceneBase {
public:
    ResultScene();
    ~ResultScene() override;

    void Initialize() override;

protected:
    void OnUpdate() override;

private:
    ScreenBuffer *screenBuffer_ = nullptr;
    ShadowMapBuffer *shadowMapBuffer_ = nullptr;
    ShadowMapBinder *shadowMapBinder_ = nullptr;

    Camera2D *screenCamera2D_ = nullptr;
    Sprite *screenSprite_ = nullptr;

    Camera3D *mainCamera3D_ = nullptr;
    Camera3D *lightCamera3D_ = nullptr;

    DirectionalLight *light_ = nullptr;

    Object3DBase *floor_ = nullptr;
    Object3DBase *sphere_ = nullptr;
};

} // namespace KashipanEngine
