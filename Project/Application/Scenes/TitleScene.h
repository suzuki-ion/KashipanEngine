#pragma once
#pragma once

#include <KashipanEngine.h>

namespace KashipanEngine {

class TitleSceneUIController;

class TitleScene final : public SceneBase {
public:
    TitleScene();
    ~TitleScene() override;

    void Initialize() override;

protected:
    void OnUpdate() override;

private:
    SceneDefaultVariables *sceneDefaultVariables_ = nullptr;
    Object3DBase *dummyPlayer_ = nullptr;
    Camera3D *mainCamera_ = nullptr;
    TitleSceneUIController *titleSceneUIController_ = nullptr;

    float moveSpeedZ_ = 24.0f;
    float cameraPlayerOffsetZ_ = 8.0f;
};

} // namespace KashipanEngine