#pragma once
#include <KashipanEngine.h>

namespace KashipanEngine {

    class EnemyDieParticle final : public IObjectComponent3D {
    public:
        struct ParticleConfig {
            int particleCount = 20;              // パーティクル数
            float initialSpeed = 5.0f;           // 初速度
            float speedVariation = 2.0f;         // 速度のランダム幅
            float lifeTimeSec = 1.0f;            // 生存時間
            float gravity = 9.8f;                // 重力加速度
            float damping = 0.95f;               // 減衰率
            Vector3 baseScale{ 1.0f, 1.0f, 1.0f }; // 基本スケール
        };

        EnemyDieParticle(const ParticleConfig& config = {});

        ~EnemyDieParticle() override = default;

        std::unique_ptr<IObjectComponent> Clone() const override {
            return std::make_unique<EnemyDieParticle>(config_);
        }

        std::optional<bool> Initialize() override;
        std::optional<bool> Update() override;

        // 死亡位置でパーティクルを発生させる
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