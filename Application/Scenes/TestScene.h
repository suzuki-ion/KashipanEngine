#pragma once
#include <KashipanEngine.h>

#include "Scenes/Components/BPM/BPMSystem.h"
#include "objects/Components/Bomb/BombManager.h"
#include "objects/Components/Bomb/explosionManager.h"
#include "Scenes/Components/PlayerHealthUI.h"

namespace KashipanEngine {

class TestScene final : public SceneBase {
public:
    TestScene();
    ~TestScene() override;

    void Initialize() override;

    /// @brief 3Dオブジェクトをシーンに追加（外部からアクセス可能）
    void AddBombObject(std::unique_ptr<Object3DBase> obj) {
        AddObject3D(std::move(obj));
    }

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
    ColliderComponent* playerCollider_ = nullptr;
	float playerScaleMin_ = 0.75f, playerScaleMax_ = 1.0f; // プレイヤーのBpmに合わせた拡大縮小範囲
	float playerBpmToleranceRange_ = 0.2f;                 // プレイヤーがBPMに合わせる±の許容範囲 
	float playerMoveDuration_ = 0.1f;                      // プレイヤー移動の所要時間（秒）

    PlayerHealthUI* playerHealthUI_ = nullptr;

	int playerMapX_ = 0; // プレイヤーのマップ上のX座標
	int playerMapZ_ = 0; // プレイヤーのマップ上のZ座標

    BPMSystem* bpmSystem_ = nullptr;
	float bpm_ = 120.0f;   // BPM値
	bool playBgm_ = false; // true-> BPM120のBGM再生

	int bombMaxNumber_ = 6; // プレイヤーが設置可能な爆弾の最大数

	BombManager* bombManager_ = nullptr;
	ExplosionManager* explosionManager_ = nullptr;
};

} // namespace KashipanEngine