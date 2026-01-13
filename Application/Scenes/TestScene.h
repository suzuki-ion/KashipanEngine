#pragma once
#include <KashipanEngine.h>

#include "Scenes/Components/BPM/BPMSystem.h"

namespace KashipanEngine {

class TestScene final : public SceneBase {
public:
    TestScene();
    ~TestScene() override;

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

    static constexpr int kMapW = 13;
    static constexpr int kMapH = 13;
    std::array<std::array<Object3DBase*, kMapW>, kMapH> maps_{};
	float mapScaleMin_ = 1.5f, mapScaleMax_ = 1.8f;
	bool allMapAnimation_ = false;

    Object3DBase* player_ = nullptr;
    float playerScaleMin_ = 0.75f, playerScaleMax_ = 1.0f;

    BPMSystem* bpmSystem_ = nullptr;
	float bpm_ = 180.0f; // BPM値
};

} // namespace KashipanEngine