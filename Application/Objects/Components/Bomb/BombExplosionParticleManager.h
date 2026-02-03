#pragma once
#include <KashipanEngine.h>
#include <vector>
#include <memory>
#include "Objects/Components/ParticleConfig.h"

namespace KashipanEngine {

class BombExplosionParticleManager final : public ISceneComponent {
public:
    BombExplosionParticleManager();
    ~BombExplosionParticleManager() override = default;

    void Initialize() override;
    void Update() override;

    void SetScreenBuffer(ScreenBuffer* screenBuffer) { screenBuffer_ = screenBuffer; }
    
    void SpawnParticles(const Vector3& position, int particleCount = 10);

    void SetParticleConfig(const ParticleConfig& config) { config_ = config; }
    const ParticleConfig& GetParticleConfig() const { return config_; }

#if defined(USE_IMGUI)
    void ShowImGui() override;
#endif

private:
    struct ParticleInfo {
        Object3DBase* object = nullptr;
        class BombExplosionParticle* component = nullptr;
    };

    ScreenBuffer* screenBuffer_ = nullptr;
    
    std::vector<ParticleInfo> particles_;
    ParticleConfig config_;
    int particlePoolSize_ = 150;
    int particleCount_ = 10;
};

} // namespace KashipanEngine
