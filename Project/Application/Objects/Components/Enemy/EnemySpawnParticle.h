#pragma once
#include <KashipanEngine.h>
#include "Objects/Components/ParticleConfig.h"

namespace KashipanEngine {

    class EnemySpawnParticle final : public IObjectComponent3D {
    public:

        EnemySpawnParticle(const ParticleConfig& config = {});

        ~EnemySpawnParticle() override = default;

        std::unique_ptr<IObjectComponent> Clone() const override {
            return std::make_unique<EnemySpawnParticle>(config_);
        }

        std::optional<bool> Initialize() override;
        std::optional<bool> Update() override;

        // スポーン位置でパーティクルを発生させる
        void Spawn(const Vector3& position);

        void SetConfig(const ParticleConfig& config) { config_ = config; }
        const ParticleConfig& GetConfig() const { return config_; }
        bool IsAlive() const;

#if defined(USE_IMGUI)
        void ShowImGui() override;
#endif

    private:
        Transform3D* transform_ = nullptr;

        ParticleConfig config_{};
        int particleCount_ = 15;

        Vector3 velocity_{ 0.0f, 0.0f, 0.0f };
        float elapsed_ = 0.0f;
        bool isActive_ = false;
        bool isAlive_ = false;
    };

} // namespace KashipanEngine
