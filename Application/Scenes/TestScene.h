#pragma once
#include <KashipanEngine.h>

#include "Scenes/Components/BPM/BPMSystem.h"
#include "Objects/SystemObjects/PointLight.h"
#include "Objects/SystemObjects/SpotLight.h"

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

	static constexpr int kMapW = 13; // マップの横幅 (オブジェクト数)
	static constexpr int kMapH = 13; // マップの縦幅 (オブジェクト数)

    std::array<std::array<Object3DBase*, kMapW>, kMapH> maps_{};
	float mapScaleMin_ = 1.5f, mapScaleMax_ = 1.8f; // マップのBpmに合わせた拡大縮小範囲
	bool allMapAnimation_ = false; // true -> 全Mapアニメーション  false-> プレイヤー位置のみアニメーション

    Object3DBase* player_ = nullptr;
	float playerScaleMin_ = 0.75f, playerScaleMax_ = 1.0f; // プレイヤーのBpmに合わせた拡大縮小範囲
	float playerBpmToleranceRange_ = 0.2f;                 // プレイヤーがBPMに合わせる±の許容範囲 
	float playerMoveDuration_ = 0.1f;                      // プレイヤー移動の所要時間（秒）

	int playerMapX_ = 0; // プレイヤーのマップ上のX座標
	int playerMapZ_ = 0; // プレイヤーのマップ上のZ座標

    BPMSystem* bpmSystem_ = nullptr;
	float bpm_ = 120.0f;   // BPM値
	bool playBgm_ = false; // true-> BPM120のBGM再生 

    struct ParticleLightPair {
        Object3DBase* particle = nullptr;
        PointLight* light = nullptr;
    };

    std::vector<ParticleLightPair> particleLights_;

    Vector4 particleLightColor_{ 1.0f, 0.85f, 0.7f, 1.0f };
    float particleLightIntensityMin_ = 0.0f;
    float particleLightIntensityMax_ = 4.0f;
    float particleLightRangeMin_ = 0.0f;
    float particleLightRangeMax_ = 5.0f;
};

} // namespace KashipanEngine