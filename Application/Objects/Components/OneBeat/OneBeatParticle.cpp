#include "OneBeatParticle.h"

namespace KashipanEngine {

	OneBeatParticle::OneBeatParticle(const ParticleConfig& config)
		: IObjectComponent3D("OneBeatParticle", 1),
		config_(config),
		isAlive_(false)
	{}

	std::optional<bool> OneBeatParticle::Initialize() {
		transform_ = GetOwner3DContext()->GetComponent<Transform3D>();
		if (!transform_) {
			return false;
		}

		// 初期状態は非表示
		transform_->SetScale(Vector3{ 0.0f, 0.0f, 0.0f });
		isActive_ = false;
		elapsed_ = 0.0f;

		config_.initialSpeed = 8.0f;               // 初速度
		config_.speedVariation = 2.0f;             // 速度のランダム幅
		config_.lifeTimeSec = 1.5f;                // 生存時間
		config_.gravity = 15.0f;                   // 重力加速度
		config_.damping = 0.98f;                   // 減衰率
		config_.spreadAngle = 30.0f;               // 広がる角度（度）
		config_.baseScale = { 1.3f, 1.3f, 1.3f };  // 基本スケール

		return true;
	}

	std::optional<bool> OneBeatParticle::Update() {
		if (!isActive_ || !transform_) {
			return true;
		}

		const float dt = std::max(0.0f, GetDeltaTime());
		elapsed_ += dt;

		// ライフタイムを超えたら非アクティブ化
		if (elapsed_ >= config_.lifeTimeSec) {
			isActive_ = false;
			transform_->SetScale(Vector3{ 0.0f, 0.0f, 0.0f });
			isAlive_ = false;
			return true;
		}

		// 物理演算: 重力と減衰
		velocity_.y -= config_.gravity * dt;
		velocity_ *= config_.damping;

		// 位置更新
		Vector3 pos = transform_->GetTranslate();
		pos += velocity_ * dt;
		transform_->SetTranslate(pos);

		// スケールアニメーション (フェードアウト)
		const float t = elapsed_ / config_.lifeTimeSec;
		const float scaleMultiplier = EaseOutCubic(1.0f, 0.0f, t);
		transform_->SetScale(config_.baseScale * scaleMultiplier);

		return true;
	}

	void OneBeatParticle::Spawn(const Vector3& position) {
		if (!transform_) return;

		// 上方向を基準にランダムな角度で広がる
		const float angleRad = GetRandomFloat(-config_.spreadAngle, config_.spreadAngle) * 0.0174533f; // 度からラジアンへ
		const float speed = config_.initialSpeed + GetRandomFloat(-config_.speedVariation, config_.speedVariation);

		// X-Z平面でのランダムな方向
		const float azimuth = GetRandomFloat(0.0f, 6.28318f); // 0〜2π

		// 上方向（Y軸正）を中心に広がる速度ベクトル
		velocity_ = Vector3{
			std::sin(angleRad) * std::cos(azimuth),
			std::cos(angleRad),
			std::sin(angleRad) * std::sin(azimuth)
		} *speed;

		// 位置とスケールを設定
		transform_->SetTranslate(position);
		transform_->SetScale(config_.baseScale);

		// アクティブ化
		isActive_ = true;
		isAlive_ = true;
		elapsed_ = 0.0f;
	}

#if defined(USE_IMGUI)
	void OneBeatParticle::ShowImGui() {
		ImGui::DragFloat("Initial Speed", &config_.initialSpeed, 0.1f, 0.0f, 20.0f);
		ImGui::DragFloat("Speed Variation", &config_.speedVariation, 0.1f, 0.0f, 10.0f);
		ImGui::DragFloat("Life Time (sec)", &config_.lifeTimeSec, 0.05f, 0.1f, 5.0f);
		ImGui::DragFloat("Gravity", &config_.gravity, 0.1f, 0.0f, 30.0f);
		ImGui::DragFloat("Damping", &config_.damping, 0.01f, 0.0f, 1.0f);
		ImGui::DragFloat("Spread Angle", &config_.spreadAngle, 1.0f, 0.0f, 90.0f);
		ImGui::DragFloat3("Base Scale", &config_.baseScale.x, 0.01f, 0.01f, 2.0f);
	}
#endif

} // namespace KashipanEngine