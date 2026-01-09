#pragma once
#include <KashipanEngine.h>

namespace KashipanEngine {

class Camera2D;
class Sprite;
class Camera3D;
class Billboard;
class Sphere;
class Model;
class Plane3D;
class CameraController;
class DirectionalLight;

class GameScene final : public SceneBase {
public:
    GameScene();
    ~GameScene() override;

protected:
    void OnUpdate() override;

private:
    ScreenBuffer *screenBuffer_ = nullptr;
    Camera2D *screenCamera2D_ = nullptr;
    Sprite *screenSprite_ = nullptr;
    Sprite *avoidCommandSprite_ = nullptr;

    Camera3D *mainCamera3D_ = nullptr;

    // MainCamera3D を制御するシーンコンポーネント
    CameraController *cameraController_ = nullptr;

    // 平行光源
    DirectionalLight *directionalLight_ = nullptr;

    // 移動用の親オブジェクト（RailMovement を登録する Sphere）
    Sphere *mover_ = nullptr;
    // 移動床
    Plane3D *floorPlane_ = nullptr;

    Sphere *player_ = nullptr;
    Model *skySphere_ = nullptr;
    std::vector<Billboard *> particleBillboards_;

    bool prevDamagedThisCooldown_ = false;
    bool prevGameProgressFinished_ = false;

    int justDodgeCount_ = 0;
    bool prevJustDodging_ = false;
    bool justDodgeCountedThisDash_ = false;

    bool prevDashTriggered_ = false;

    AudioManager::PlayHandle bgmPlay_ = AudioManager::kInvalidPlayHandle;
    AudioManager::PlayHandle avoidPlay_ = AudioManager::kInvalidPlayHandle;
    AudioManager::PlayHandle avoidJustPlay_ = AudioManager::kInvalidPlayHandle;
    AudioManager::PlayHandle damagePlay_ = AudioManager::kInvalidPlayHandle;
};

} // namespace KashipanEngine
