#pragma once
#include <KashipanEngine.h>
#include <memory>
#include <vector>
#include "Scenes/Components/BPM/BPMSystem.h"
#include "OneBeatParticle.h"

namespace KashipanEngine {

	class OneBeatEmitter final : public ISceneComponent {
	public:
		explicit OneBeatEmitter()
			: ISceneComponent("OneBeatEmitter", 10) {}

		~OneBeatEmitter() override = default;

		void Initialize() override;
		void Update() override;

		void SetScreenBuffer(ScreenBuffer* screenBuffer) { screenBuffer_ = screenBuffer; }
		void SetShadowMapBuffer(ShadowMapBuffer* shadowMapBuffer) { shadowMapBuffer_ = shadowMapBuffer; }
		void SetBPMSystem(BPMSystem* bpmSystem) { bpmSystem_ = bpmSystem; }
		void SetEmitter(Object3DBase* emitter) { emitter_ = emitter; }

		/// @brief パーティクルプールを初期化
		/// @param particlesPerBeat 1拍ごとに発生するパーティクル数
		void InitializeParticlePool(int particlesPerBeat = 10);

		void SetParticleConfig(const ParticleConfig& config) { config_ = config; }

		void SetUseEmitter(bool useEmitter) { useEmitter_ = useEmitter; }
	private:
		/// @brief 一拍ごとに火花を発生させる
		void SpawnSparks();

		std::vector<Object3DBase*> particlePool_;
		static constexpr int kParticlePoolSize_ = 10;

		ScreenBuffer* screenBuffer_ = nullptr;
		ShadowMapBuffer* shadowMapBuffer_ = nullptr;
		BPMSystem* bpmSystem_ = nullptr;
		Object3DBase* emitter_ = nullptr;

		ParticleConfig config_;

		int lastBeat_ = -1;
		int particlesPerBeat_ = 10;

		bool useEmitter_ = true;
	};

} // namespace KashipanEngine