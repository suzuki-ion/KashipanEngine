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
};

} // namespace KashipanEngine
