#pragma once
#include <KashipanEngine.h>

namespace KashipanEngine {

class CollisionTestScene final : public Scene {
public:
    CollisionTestScene();
    ~CollisionTestScene() override;

    void Initialize() override;

protected:
    void OnUpdate() override;

private:
    void RespawnAreaObject(EmptyObject *obj);
    SceneDefaultVariables *sceneDefaultVariables_ = nullptr;
    std::vector<EmptyObject *> areaObjects_;

    Vector3 areaSize_ = Vector3(128.0f, 32.0f, 128.0f);
};

} // namespace KashipanEngine
