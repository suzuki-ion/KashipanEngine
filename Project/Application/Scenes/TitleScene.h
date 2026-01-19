#pragma once
#include <KashipanEngine.h>

namespace KashipanEngine {

class Camera2D;
class Sprite;

class Camera3D;
class Plane3D;
class Sphere;

class TitleScene final : public SceneBase {
public:
    TitleScene();
    ~TitleScene() override;

protected:
    void OnUpdate() override;

private:
    // プレイヤー
    Sphere *player_ = nullptr;

    // プレイヤー移動範囲
    Vector3 playerMoveMin_ = Vector3{-5.0f, 0.0f, -2.0f};
    Vector3 playerMoveMax_ = Vector3{5.0f, 0.0f, 3.0f};

    AudioManager::PlayHandle bgmPlay_ = AudioManager::kInvalidPlayHandle;
};

} // namespace KashipanEngine