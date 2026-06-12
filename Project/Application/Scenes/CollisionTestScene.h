#pragma once
#include <KashipanEngine.h>

namespace KashipanEngine {

class CollisionTestScene final : public SceneBase {
public:
    CollisionTestScene();
    ~CollisionTestScene() override;

    void Initialize() override;

protected:
    void OnUpdate() override;

private:
    SceneDefaultVariables *sceneDefaultVariables_ = nullptr;
};

} // namespace KashipanEngine
