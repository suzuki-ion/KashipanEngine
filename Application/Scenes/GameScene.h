#pragma once
#include <KashipanEngine.h>

#include "Scenes/Components/BPM/BPMSystem.h"
#include "Scenes/Components/WaveSystem.h"
#include "objects/Components/Bomb/BombManager.h"
#include "objects/Components/Bomb/explosionManager.h"
#include "objects/Components/Bomb/ScoreDisplay.h"
#include "objects/Components/Bomb/BombExplosionParticleManager.h"
#include "Scenes/Components/PlayerHealthUI.h"
#include "Scenes/Components/PlayerHealthModelUI.h"
#include "Objects/Components/Enemy/EnemyManager.h"
#include "Objects/Components/Enemy/EnemySpawner.h"
#include "Objects/Components/Player/ScoreManager.h"
#include "Scene/Components/ColliderComponent.h"
#include "Objects/SystemObjects/PointLight.h"
#include "Objects/SystemObjects/SpotLight.h"
#include "Objects/SystemObjects/VelocityBufferCameraBinder.h"
#include "Objects/Components/OneBeat/OneBeatParticle.h"
#include "Objects/Components/OneBeat/OneBeatEmitter.h"
#include "Objects/Components/Enemy/EnemyDieParticle.h"
#include "Objects/Components/Player/PlayerDieParticleManager.h"
#include "Scenes/Components/CameraController.h"
#include "Scenes/Components/BackMonitor.h"
#include "Scenes/Components/BackMonitorWithGameScreen.h"
#include "Scenes/Components/BackMonitorWithMenuScreen.h"
#include "Scenes/Components/BackMonitorWithParticle.h"
#include "Scenes/Components/StageLighting.h"
#include "Utilities/Json/JsonManager.h"
#include "Objects/Components/Bomb/ExplosionManager.h"
#include "Objects/Components/Map/WallInfo.h"
#include "Objects/Components/Player/PlayerBombSpawnMode.h"

namespace KashipanEngine {

class GameScene final : public SceneBase {
public:
    GameScene();
    ~GameScene() override;

    void Initialize() override;

private:
    void LoadObjectStateJson();
    void SaveObjectStateJson();

    void LoadParticleStateJson();
    void SaveParticleStateJson();

    /// @brief 壁の再生成待機中の場所を可視化するためにマップマーカーを更新
    void UpdateWallRespawnMarkers();

    /// @brief 壁の後ろにオブジェクトがある場合、壁を半透明にする
    void UpdateWallTransparency();

	/// @brief  Waveシステム初期化
    void InitWaveSystem(ScreenBuffer* screenBuffer, Transform3D* transform);
#if defined(USE_IMGUI)
    void DrawObjectStateImGui();
    void DrawParticleStateImGui();
#endif
    void SetObjectValue();
    void SetParticleValue();

    void InGameStart();
	void InGameQuit();
private:
    SceneDefaultVariables *sceneDefaultVariables_ = nullptr;

    std::unique_ptr<JsonManager> jsonManager_;
    std::string loadToSaveName_ = "ObjectState";

    std::unique_ptr<JsonManager> jsonParticleManager_;
    std::string loadToSaveParticleName_ = "particleState";

    Object3DBase *stage_ = nullptr;

    Object3DBase* djNagasawa_ = nullptr;

	bool isGameStarted_ = false; // ゲーム開始フラグ

    // BPMオブジェクトのBPMに合わせた拡大縮小範囲
    static constexpr int kBpmObjectCount = 4;  // BPMオブジェクトの数
    Object3DBase *bpmObjects_[kBpmObjectCount]{};
    Vector3 bpmObjectStart_[kBpmObjectCount]{}, bpmObjectEnd_[kBpmObjectCount]{};

    // OneBeat パーティクルエミッター関連
    static constexpr int kOneBeatEmitterCount_ = 10; // OneBeatエミッターの数
    Object3DBase *oneBeatEmitterObj_[kOneBeatEmitterCount_]{};
    OneBeatEmitter *oneBeatEmitter_[kOneBeatEmitterCount_]{};
    Vector3 emitterTranslate_[kOneBeatEmitterCount_]{};
    int particlesPerBeat_ = 10; // 1拍ごとに発生するパーティクル数

    // BPM関連
    BPMSystem *bpmSystem_ = nullptr;
    int bpm_ = 120;   // BPM値
    
    AudioManager::PlayHandle bgmPlayHandle_ = AudioManager::kInvalidPlayHandle;

    // マップ関連
    static constexpr int kMapW = 10;                // マップの横幅 (オブジェクト数)
    static constexpr int kMapH = 10;                // マップの縦幅 (オブジェクト数)
    Vector3 mapScaleMin_ = { 0.65f ,0.95f ,0.95f }, mapScaleMax_ = { 0.95f ,0.95f ,0.95f }; // マップのBpmに合わせた拡大縮小範囲

    bool allMapAnimation_ = false; // true -> 全Mapアニメーション  false-> プレイヤー位置のみアニメーション
    std::array<std::array<Object3DBase *, kMapW>, kMapH> maps_{};
    std::array<std::array<Object3DBase *, kMapW>, kMapH> mapMarkers_{};
    std::array<std::array<bool, kMapW>, kMapH> mapMarkerIsActive_{};

    std::array<std::array<WallInfo, kMapW>, kMapH> walls_{};
	float wallAlpha_ = 0.5f; // 壁の透明度
    
    CameraController *cameraController_ = nullptr;
    float pDamageShakePower_ = 5.0f; float pDamageShakeTime_ = 1.0f; // プレイヤーダメージ時のカメラシェイク
    float bombShakePower_ = 5.0f; float bombShakeTime_ = 1.0f; // 爆弾爆発時のカメラシェイク

    // プレイヤー関連
    Object3DBase *player_ = nullptr;
    Vector3 playerStartPos_ = { 10.0f,0.0f,10.0f };
    Vector3 playerScaleMin_ = { 1.1f, 0.75f,1.1f }, playerScaleMax_ = { 1.0f ,1.0f ,1.0f };// プレイヤーのBpmに合わせた拡大縮小範囲
    float playerBpmToleranceRange_ = 0.1f;                 // プレイヤーがBPMに合わせる±の許容範囲 
    float playerNoneBpmToleranceRange_ = 0.25f;           // プレイヤーがチェインBPMに合わせる±の許容範囲
	float playerChainBpmToleranceRange_ = 0.25f;           // プレイヤーがチェインBPMに合わせる±の許容範囲
    float playerMoveDuration_ = 0.1f;                      // プレイヤー移動の所要時間（秒）

	float playerMoveInputInterval_ = 0.0f;                 // プレイヤー移動入力のインターバル時間（秒）
	float playerNoneMoveInputInterval_ = 0.4f;             // プレイヤー移動入力のインターバル時間（秒）（BPM外）
	float playerChainMoveInputInterval_ = 0.25f;             // プレイヤー移動入力のインターバル時間（秒）（チェインBPM外）

    bool isMoveBombStop_ = false;
    bool usePlayerDirection_ = false;
    bool enablePlayerDestructiveKnockback_ = false;  // プレイヤーの破壊的ノックバックを有効化

    int playerMapX_ = 0; // プレイヤーのマップ上のX座標
    int playerMapZ_ = 0; // プレイヤーのマップ上のZ座標
    PlayerHealthModelUI *playerHealthUI_ = nullptr;

    // 爆弾関連
    BombManager *bombManager_ = nullptr;
    int bombMaxNumber_ = 1000;     // プレイヤーが設置可能な爆弾の最大数
    int bombLifetimeBeats_ = 1000; // 設置してからの爆弾の寿命（拍数）
    int bombMaxChainCount_ = 10;   // Chainモードで連鎖できる最大爆弾数

    // 爆発関連
    ExplosionManager *explosionManager_ = nullptr;
    BombExplosionParticleManager *bombExplosionParticleManager_ = nullptr;
    ScoreDisplay *explosionNumberDisplay_ = nullptr;
    float explosionLifetime_ = 0.5f; // 爆発の寿命（秒）
    int explosionSize_ = 3;         // 爆発のXZサイズ
	bool isBreakWalls_ = true;    // 爆発で壁を壊すかどうか

    // ExplosionNumberDisplay関連
    float explosionNumberDisplayLifetime_ = 1.0f; // 数字の表示時間（秒）
    float explosionNumberScale_ = 1.0f;           // 数字のスケール
    float explosionNumberYOffset_ = 1.0f;         // Y方向のオフセット

    float bombNormalMinScale_ = 0.9f; // 最小スケール係数
    float bombNormalMaxScale_ = 1.1f;  // 最大スケール係数
    float bombSpeedMinScale_ = 0.8f;   // 最小スケール係数
    float bombSpeedMaxScale_ = 1.0f;   // 最大スケール係数
    float bombDetonationScale_ = 1.5f; // 起爆時のスケール係数

    // 敵関連
    EnemySpawner *enemySpawner_ = nullptr;
    int enemySpawnInterval_ = 4; // 敵のスポーン間隔（拍数）

    EnemyManager *enemyManager_ = nullptr;

    // Wave関連
    WaveSystem *waveSystem_ = nullptr;

	int wallSpawnAgainCount_ = 8; // 壁の再生成までの待機拍数

    // プレイヤー死亡パーティクル関連
    PlayerDieParticleManager *playerDieParticleManager_ = nullptr;

    // スコア関連
    ScoreManager *scoreManager_ = nullptr;

    struct ParticleLightPair {
        Object3DBase *particle = nullptr;
        PointLight *light = nullptr;
    };
private:
    // パーティクル機能に渡す値
    ParticleConfig enemySpawnParticleConfig_{};
    ParticleConfig enemyDieParticleConfig_{};
    ParticleConfig oneBeatParticleConfig_{};
    ParticleConfig oneBeatMissParticleConfig_{};
    ParticleConfig playerDieParticleConfig_{};

    int enemySpawnParticleCount_ = 20;
    int enemyDieParticleCount_ = 30;
    int oneBeatParticleCount_ = 50;
    int playerDieParticleCount_ = 10;

    std::vector<ParticleLightPair> particleLights_;

    Vector4 particleLightColor_{ 1.0f, 0.85f, 0.7f, 1.0f };
    float particleLightIntensityMin_ = 0.0f;
    float particleLightIntensityMax_ = 4.0f;
    float particleLightRangeMin_ = 0.0f;
    float particleLightRangeMax_ = 5.0f;

private:
    // BackMonitor related (debug test)
    BackMonitorWithGameScreen *backMonitorGame_ = nullptr;
    BackMonitorWithMenuScreen *backMonitorMenu_ = nullptr;
    BackMonitorWithParticle *backMonitorParticle_ = nullptr;
    int backMonitorMode_ = 0; // 0=game,1=menu,2=particle

    // ステージライティング
    StageLighting *stageLighting_ = nullptr;

    // カメラ位置
    Vector3 cameraGameTargetPos_{};
    Vector3 cameraGameTargetRot_{};
    Vector3 cameraMenuTargetPos_{};
    Vector3 cameraMenuTargetRot_{};

protected:
    void OnUpdate() override;
};

} // namespace KashipanEngine