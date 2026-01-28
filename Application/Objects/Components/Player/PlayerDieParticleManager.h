#pragma once
#include <KashipanEngine.h>
#include <vector>
#include "Objects/Components/ParticleConfig.h"

namespace KashipanEngine {

class Health;
class PlayerDieParticle;

class PlayerDieParticleManager final : public ISceneComponent {
public:
    PlayerDieParticleManager();
    ~PlayerDieParticleManager() override = default;

    void Update() override;

    void SetScreenBuffer(ScreenBuffer* buffer) { screenBuffer_ = buffer; }
    void SetShadowMapBuffer(ShadowMapBuffer* buffer) { shadowMapBuffer_ = buffer; }
    void SetPlayer(Object3DBase* player) { player_ = player; }
    void SetParticleConfig(const ParticleConfig& config) { particleConfig_ = config; }

    void InitializeParticlePool(int count = 5);
    void SpawnParticles(const Vector3& position);

private:
    ScreenBuffer* screenBuffer_ = nullptr;
    ShadowMapBuffer* shadowMapBuffer_ = nullptr;
    Object3DBase* player_ = nullptr;

    ParticleConfig particleConfig_{};
    int particleCount_ = 30;

    std::vector<Object3DBase*> particlePool_;
    bool wasAlive_ = true;
};

} // namespace KashipanEngine
