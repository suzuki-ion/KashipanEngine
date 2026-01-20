#pragma once
#include <KashipanEngine.h>

namespace KashipanEngine {

class MenuScene final : public SceneBase {
public:
    MenuScene();
    ~MenuScene() override;

    void Initialize() override;

protected:
    void OnUpdate() override;

private:
    SceneDefaultVariables *sceneDefaultVariables_ = nullptr;
};

} // namespace KashipanEngine
