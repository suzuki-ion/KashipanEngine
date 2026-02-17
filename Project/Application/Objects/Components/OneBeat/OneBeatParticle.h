#pragma once
#include <KashipanEngine.h>
#include "Objects/Components/ParticleConfig.h"

namespace KashipanEngine {

	class BPMSystem;

	class OneBeatParticle final : public IObjectComponent3D {
	public:

		OneBeatParticle(const ParticleConfig& config = {});

		~OneBeatParticle() override = default;

		std::unique_ptr<IObjectComponent> Clone() const override {
			return std::make_unique<OneBeatParticle>(config_);
		}

		std::optional<bool> Initialize() override;
		std::optional<bool> Update() override;

		/// @brief 火花を発生させる
		void Spawn(const Vector3& position);

		void SetConfig(const ParticleConfig& config) { config_ = config; }
		void SetMissConfig(const ParticleConfig& config) { missConfig_ = config; }

		const ParticleConfig& GetConfig() const { return config_; }
		bool IsAlive() const { return isAlive_; }

#if defined(USE_IMGUI)
		void ShowImGui() override;
#endif

	private:
		Transform3D* transform_ = nullptr;
        Material3D *material_ = nullptr;

		ParticleConfig config_{};
		ParticleConfig missConfig_{};
		
		Vector3 velocity_{ 0.0f, 0.0f, 0.0f };
		float elapsed_ = 0.0f;
		bool isActive_ = false;
		bool isAlive_ = false;
	};

} // namespace KashipanEngine