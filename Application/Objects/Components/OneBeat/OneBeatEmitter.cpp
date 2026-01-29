#include "OneBeatEmitter.h"

namespace KashipanEngine {

void OneBeatEmitter::Initialize() {
	ISceneComponent::Initialize();
	lastBeat_ = -1;  // 初期化を明示的に行う
}

void OneBeatEmitter::InitializeParticlePool(int particlesPerBeat) {
	auto* ctx = GetOwnerContext();
	if (!ctx) return;

	particlesPerBeat_ = particlesPerBeat;
	particlePool_.reserve(kParticlePoolSize_);

	for (int i = 0; i < kParticlePoolSize_; ++i) {
		// パーティクルオブジェクトを生成
		auto particle = std::make_unique<Box>();
		particle->SetName("OneBeatParticle_" + std::to_string(i));

		// Transform設定
		if (auto* tr = particle->GetComponent3D<Transform3D>()) {
			tr->SetScale(Vector3{ 0.0f, 0.0f, 0.0f }); // 初期は非表示
		}

		// Material設定 (オレンジ色の火花)
		if (auto* mat = particle->GetComponent3D<Material3D>()) {
			mat->SetColor(Vector4{ 1.0f, 0.5f, 0.0f, 1.0f });
			mat->SetEnableLighting(true);
		}

		// OneBeatParticleコンポーネント追加
		config_.initialSpeed = 8.0f;
		config_.speedVariation = 2.0f;
		config_.lifeTimeSec = 0.5f;
		config_.gravity = 15.0f;
		config_.damping = 0.98f;
		config_.spreadAngle = 30.0f;
		config_.baseScale = Vector3{ 0.3f, 0.3f, 0.3f };

		particle->RegisterComponent<OneBeatParticle>(config_);

		// レンダラーにアタッチ
		if (screenBuffer_) {
			particle->AttachToRenderer(screenBuffer_, "Object3D.Solid.BlendNormal");
		}
		if (shadowMapBuffer_) {
			//particle->AttachToRenderer(shadowMapBuffer_, "Object3D.ShadowMap.DepthOnly");
		}

		particlePool_.push_back(particle.get());
		ctx->AddObject3D(std::move(particle));
	}
}

void OneBeatEmitter::Update() {
	if (!bpmSystem_) {
		return;
	}

	if (!emitter_) {
		return;
	}

	if (bpmProgress_ <= 0.0f + bpmToleranceRange_ || bpmProgress_ >= 1.0f - bpmToleranceRange_) {
		if (inputCommand_->Evaluate("MoveUp").Triggered()) {
			isMissBeat_ = false;
		} else if (inputCommand_->Evaluate("MoveDown").Triggered()) {
			isMissBeat_ = false;
		} else if (inputCommand_->Evaluate("MoveLeft").Triggered()) {
			isMissBeat_ = false;
		} else if (inputCommand_->Evaluate("MoveRight").Triggered()) {
			isMissBeat_ = false;
		} else if (inputCommand_->Evaluate("Bomb").Triggered()) {
			isMissBeat_ = false;
		}
	}

	// 一拍ごとに火花を発生
	int currentBeat = bpmSystem_->GetCurrentBeat();
	if (currentBeat != lastBeat_) {
		lastBeat_ = currentBeat;
		if (useEmitter_) {
			SpawnSparks();
		}
		isMissBeat_ = true;
	}
}

void OneBeatEmitter::SpawnSparks() {
	if (!emitter_) return;

	config_.color = baseColor_;
	missConfig_.color = missColor_;

	// プール内の全パーティクルにconfigを適用
	for (auto* particle : particlePool_) {
		if (auto* oneBeatParticle = particle->GetComponent3D<OneBeatParticle>()) {
			if (isMissBeat_) {
				oneBeatParticle->SetConfig(missConfig_);
			} else {
				oneBeatParticle->SetConfig(config_);
			}
		}
	}

	// エミッターの位置を取得
	Vector3 emitterPosition{ 0.0f, 0.0f, 0.0f };
	if (auto* tr = emitter_->GetComponent3D<Transform3D>()) {
		emitterPosition = tr->GetTranslate();
	}

	// プールから非アクティブなパーティクルを探して再利用
	int particlesSpawned = 0;

	for (auto* particle : particlePool_) {
		if (particlesSpawned >= particlesPerBeat_) break;

		auto* oneBeatParticle = particle->GetComponent3D<OneBeatParticle>();

		// 非アクティブなパーティクルのみ再利用
		if (!oneBeatParticle->IsAlive()) {
			oneBeatParticle->Spawn(emitterPosition);
			particlesSpawned++;
		}
	}
}

} // namespace KashipanEngine