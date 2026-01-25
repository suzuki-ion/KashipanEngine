#pragma once
#include <KashipanEngine.h>

#include "Scenes/Components/BPM/BPMSystem.h"
#include "objects/Components/Bomb/BombManager.h"
#include "objects/Components/Bomb/explosionManager.h"
#include "Scenes/Components/PlayerHealthUI.h"
#include "Objects/Components/Enemy/EnemyManager.h"
#include "Objects/Components/Enemy/EnemySpawner.h"
#include "Scene/Components/ColliderComponent.h"
#include "Objects/SystemObjects/PointLight.h"
#include "Objects/SystemObjects/SpotLight.h"
#include "Objects/SystemObjects/VelocityBufferCameraBinder.h"
#include "Scenes/Components/CameraController.h"
#include "Scenes/Components/BackMonitor.h"

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
#if defined(USE_IMGUI)
    void DrawImGui();
#endif
private:
    SceneDefaultVariables *sceneDefaultVariables_ = nullptr;

    Object3DBase* stage_ = nullptr;

    // BPM関連
    BPMSystem* bpmSystem_ = nullptr;
    int bpm_ = 120;   // BPM値
    bool playBgm_ = true;  // true-> BPM120のBGM再生

	// マップ関連
	static constexpr int kMapW = 11;                // マップの横幅 (オブジェクト数)
	static constexpr int kMapH = 11;                // マップの縦幅 (オブジェクト数)
    Vector3 mapScaleMin_ = { 0.65f ,0.95f ,0.95f }, mapScaleMax_ = { 0.95f ,0.95f ,0.95f }; // マップのBpmに合わせた拡大縮小範囲

    bool allMapAnimation_ = false; // true -> 全Mapアニメーション  false-> プレイヤー位置のみアニメーション
    std::array<std::array<Object3DBase*, kMapW>, kMapH> maps_{};
    
	CameraController* cameraController_ = nullptr;
	float pDamageShakePower_ = 0.0f; float pDamageShakeTime_ = 0.0f; // プレイヤーダメージ時のカメラシェイク
	float bombShakePower_    = 0.0f; float bombShakeTime_    = 0.0f; // 爆弾爆発時のカメラシェイク
	float eDieShakePower_    = 0.0f; float eDieShakeTime_    = 0.0f; // 敵マップ外死亡時のカメラシェイク

	// プレイヤー関連
    Object3DBase* player_ = nullptr;
    Vector3 playerScaleMin_ = { 1.1f, 0.75f,1.1f }, playerScaleMax_ = { 1.0f ,1.0f ,1.0f };// プレイヤーのBpmに合わせた拡大縮小範囲
	float playerBpmToleranceRange_ = 0.2f;                 // プレイヤーがBPMに合わせる±の許容範囲 
	float playerMoveDuration_ = 0.1f;                      // プレイヤー移動の所要時間（秒）

    int playerMapX_ = 0; // プレイヤーのマップ上のX座標
    int playerMapZ_ = 0; // プレイヤーのマップ上のZ座標
    PlayerHealthUI* playerHealthUI_ = nullptr;

	// 爆弾関連
	BombManager* bombManager_ = nullptr;
	int bombMaxNumber_ = 3;     // プレイヤーが設置可能な爆弾の最大数
	int bombLifetimeBeats_ = 4; // 設置してからの爆弾の寿命（拍数）

	// 爆発関連
	ExplosionManager* explosionManager_ = nullptr;
	float explosionLifetime_ = 0.5f; // 爆発の寿命（秒）

	// 敵関連
	EnemySpawner* enemySpawner_ = nullptr;
	int enemySpawnInterval_ = 4; // 敵のスポーン間隔（拍数）

	EnemyManager* enemyManager_ = nullptr;

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