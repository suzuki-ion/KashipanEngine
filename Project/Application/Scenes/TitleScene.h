#pragma once
#include <KashipanEngine.h>

namespace KashipanEngine {

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

    Text *titleText_ = nullptr;
    Text *startText_ = nullptr;
    Text *quitText_ = nullptr;

    int selectionIndex_ = 0; // 0: Start, 1: Quit
    float moveSpeedZ_ = 24.0f;
    float cameraPlayerOffsetZ_ = 8.0f;
};

} // namespace KashipanEngine