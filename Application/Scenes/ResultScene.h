#pragma once
#include <KashipanEngine.h>

namespace KashipanEngine {

class Camera2D;
class Sprite;
class Camera3D;

class ResultScene final : public SceneBase {
public:
    ResultScene();
    ~ResultScene() override;

protected:
    void OnUpdate() override;

private:
    ScreenBuffer *screenBuffer_ = nullptr;
    Camera2D *screenCamera2D_ = nullptr;
    Sprite *screenSprite_ = nullptr;

    Camera3D *mainCamera3D_ = nullptr;
};

} // namespace KashipanEngine
