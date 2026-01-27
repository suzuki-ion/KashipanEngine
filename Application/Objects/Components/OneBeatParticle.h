#pragma once
#include <KashipanEngine.h>

namespace KashipanEngine {

class BPMSystem;

class OneBeatParticle final : public IObjectComponent3D {
public:
	struct ParticleConfig {
		float initialSpeed = 8.0f;               // 初速度
		float speedVariation = 2.0f;             // 速度のランダム幅
		float lifeTimeSec = 0.5f;                // 生存時間
		float gravity = 15.0f;                   // 重力加速度
		float damping = 0.98f;                   // 減衰率
		float spreadAngle = 30.0f;               // 広がる角度（度）
		Vector3 baseScale{ 0.3f, 0.3f, 0.3f };   // 基本スケール
	};

	OneBeatParticle(const ParticleConfig& config = {});

	~OneBeatParticle() override = default;

	std::unique_ptr<IObjectComponent> Clone() const override {
		return std::make_unique<OneBeatParticle>(config_);
	}

	std::optional<bool> Initialize() override;
	std::optional<bool> Update() override;

	/// @brief BPMSystemを設定
	void SetBPMSystem(BPMSystem* bpmSystem) { bpmSystem_ = bpmSystem; }

	void SetConfig(const ParticleConfig& config) { config_ = config; }
	const ParticleConfig& GetConfig() const { return config_; }
	bool IsAlive() const { return isAlive_; }

#if defined(USE_IMGUI)
	void ShowImGui() override;
#endif

private:
	/// @brief 火花を発生させる
	void SpawnSpark();

	Transform3D* transform_ = nullptr;
	BPMSystem* bpmSystem_ = nullptr;

	ParticleConfig config_{};
	Vector3 velocity_{ 0.0f, 0.0f, 0.0f };
	float elapsed_ = 0.0f;
	bool isActive_ = false;
	bool isAlive_ = false;
	int lastBeat_ = -1;
};

} // namespace KashipanEngine