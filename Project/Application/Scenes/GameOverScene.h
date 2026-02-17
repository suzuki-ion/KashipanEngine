#pragma once
#include <KashipanEngine.h>

namespace KashipanEngine {

class GameOverScene final : public SceneBase {
public:
    GameOverScene();
    ~GameOverScene() override;

    void Initialize() override;

protected:
    void OnUpdate() override;

private:
    SceneDefaultVariables *sceneDefaultVariables_ = nullptr;
};

} // namespace KashipanEngine
