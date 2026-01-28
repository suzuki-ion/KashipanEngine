#pragma once
#include "BackMonitorRenderer.h"

namespace KashipanEngine {

class BackMonitorWithParticle : public BackMonitorRenderer {
public:
    BackMonitorWithParticle(ScreenBuffer* target);
    ~BackMonitorWithParticle() override;

    void Initialize() override;
    void Update() override;

private:
    void EnsureParticlePool();
    std::vector<Object2DBase*> particles_;

    float elapsedTime_ = 0.0f;
};

} // namespace KashipanEngine
