#pragma once
#include <KashipanEngine.h>

namespace KashipanEngine {

class TestScene final : public SceneBase {
public:
    TestScene();
    ~TestScene() override;

    void Initialize() override;

protected:
    void OnUpdate() override;

private:
    struct ParticleLightPair {
        Object3DBase* particle = nullptr;
        PointLight* light = nullptr;
    };

    std::vector<ParticleLightPair> particleLights_;

    Vector4 particleLightColor_{ 1.0f, 0.85f, 0.7f, 1.0f };
    float particleLightIntensityMin_ = 0.0f;
    float particleLightIntensityMax_ = 4.0f;
    float particleLightRangeMin_ = 0.0f;
    float particleLightRangeMax_ = 5.0f;
};

} // namespace KashipanEngine