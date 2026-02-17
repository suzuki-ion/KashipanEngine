#pragma once
#include <KashipanEngine.h>
#include "Scenes/Components/ResultScene/ResultSceneAnimator.h"

namespace KashipanEngine {

class ResultScene final : public SceneBase {
public:
    ResultScene();
    ~ResultScene() override;

    void Initialize() override;

protected:
    void OnUpdate() override;

private:
    SceneDefaultVariables *sceneDefaultVariables_ = nullptr;

    ResultSceneAnimator *resultSceneAnimator_ = nullptr;
};

} // namespace KashipanEngine
