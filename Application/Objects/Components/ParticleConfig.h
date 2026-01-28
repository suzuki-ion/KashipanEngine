#pragma once
#include <KashipanEngine.h>

namespace KashipanEngine {

	struct ParticleConfig {
		float initialSpeed = 8.0f;               // 初速度
		float speedVariation = 2.0f;             // 速度のランダム幅
		float lifeTimeSec = 1.5f;                // 生存時間
		float gravity = 15.0f;                   // 重力加速度
		float damping = 0.98f;                   // 減衰率
		float spreadAngle = 30.0f;               // 広がる角度（度）
		Vector3 baseScale{ 1.3f, 1.3f, 1.3f };   // 基本スケール
	};

} // namespace KashipanEngine