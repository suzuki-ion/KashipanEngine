#pragma once
#include <KashipanEngine.h>

namespace KashipanEngine {

class ResultScene final : public Scene {
public:
    ResultScene();
    ~ResultScene() override;

    void Initialize() override;

protected:
    void OnUpdate() override;

private:
    SceneDefaultVariables *sceneDefaultVariables_ = nullptr;
};

} // namespace KashipanEngine
