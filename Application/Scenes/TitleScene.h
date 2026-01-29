#pragma once
#include <KashipanEngine.h>
#include "Scenes/Components/TitleScene/TitleSceneAnimator.h"

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

    TitleSceneAnimator *titleSceneAnimator_ = nullptr;
};

} // namespace KashipanEngine