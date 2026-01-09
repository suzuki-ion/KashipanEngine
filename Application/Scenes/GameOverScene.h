#pragma once
#include <KashipanEngine.h>

namespace KashipanEngine {

class Camera2D;
class Sprite;
class Camera3D;
class Sphere;

class GameOverScene final : public SceneBase {
public:
    GameOverScene();
    ~GameOverScene() override;

protected:
    void OnUpdate() override;

private:
    ScreenBuffer *screenBuffer_ = nullptr;
    Camera2D *screenCamera2D_ = nullptr;
    Sprite *screenSprite_ = nullptr;

    Camera3D *mainCamera3D_ = nullptr;
    Sphere *player_ = nullptr;

    AudioManager::PlayHandle bgmPlay_ = AudioManager::kInvalidPlayHandle;
};

} // namespace KashipanEngine
