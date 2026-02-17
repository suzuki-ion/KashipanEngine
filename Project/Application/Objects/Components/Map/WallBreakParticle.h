#pragma once
#include <KashipanEngine.h>
#include "Objects/Components/ParticleConfig.h"

namespace KashipanEngine {

    class WallBreakParticle final : public IObjectComponent3D {
    public:

        WallBreakParticle(const ParticleConfig& config = {});

        ~WallBreakParticle() override = default;

        std::unique_ptr<IObjectComponent> Clone() const override {
            return std::make_unique<WallBreakParticle>(config_);
        }

        std::optional<bool> Initialize() override;
        std::optional<bool> Update() override;

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

        Vector3 velocity_{ 0.0f, 0.0f, 0.0f };
        float elapsed_ = 0.0f;
        bool isActive_ = false;
        bool isAlive_ = false;
    };

} // namespace KashipanEngine
