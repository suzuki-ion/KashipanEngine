#pragma once
#include <KashipanEngine.h>

namespace KashipanEngine {

    class EnemySpawnParticle final : public IObjectComponent3D {
    public:
        struct ParticleConfig {
            int particleCount = 15;              // パーティクル数
            float initialSpeed = 3.0f;           // 初速度（上昇）
            float speedVariation = 1.5f;         // 速度のランダム幅
            float lifeTimeSec = 1.0f;            // 生存時間（1拍分）
            float gravity = -5.0f;               // 上昇加速度（負の重力）
            float damping = 0.98f;               // 減衰率
            Vector3 baseScale{ 0.25f, 0.25f, 0.25f }; // 基本スケール
        };

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
        Vector3 velocity_{ 0.0f, 0.0f, 0.0f };
        float elapsed_ = 0.0f;
        bool isActive_ = false;
        bool isAlive_ = false;
    };

} // namespace KashipanEngine
