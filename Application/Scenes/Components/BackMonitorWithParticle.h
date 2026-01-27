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

    // Internal frame counter for simple animations
    int frameCounter_ = 0;
};

} // namespace KashipanEngine
