#pragma once
#pragma once
#include <KashipanEngine.h>
#include "Objects/Components/GroundDefined.h"

namespace KashipanEngine {
	class GravityGroundDefined final : public IObjectComponent3D {
	public:
		explicit GravityGroundDefined(const Vector3& gravityDirection)
			: IObjectComponent3D("GravityGroundDefined", 1), GravityDirection_(gravityDirection) {
		}
		~GravityGroundDefined() override = default;

		std::unique_ptr<IObjectComponent> Clone() const override {
			return std::make_unique<GravityGroundDefined>(GravityDirection_);
		}

		std::optional<bool> Initialize() override {
			auto* ctx = GetOwner3DContext();
			if (!ctx) return false;

			if (auto* gd = ctx->GetComponent<GroundDefined>()) {
				originalDefaultColor_ = gd->GetDefaultColor();
				originalTouchColorStart_ = gd->GetTouchColorStart();
				originalTouchColorEnd_ = gd->GetTouchColorEnd();

				gd->SetDefaultColor(defaultColor_);
				gd->SetTouchColorStart(touchColorStart_);
				gd->SetTouchColorEnd(touchColorEnd_);
			}

			if (auto* tr = ctx->GetComponent<Transform3D>()) {
				// Z軸を中心に回転するため、回転半径はトランスフォームの平面上の位置から計算
				const Vector3 pos = tr->GetTranslate();
				rotationRadius_ = std::sqrt(pos.x * pos.x + pos.y * pos.y);
				currentRotation_ = std::atan2(pos.y, pos.x);
				tr->SetRotate(Vector3{ 0.0f, 0.0f, currentRotation_ });

				Vector3 newPos =
					Vector3{ std::cos(currentRotation_) * rotationRadius_, std::sin(currentRotation_) * rotationRadius_, pos.z };
				tr->SetTranslate(newPos);
			}

			return true;
		};

		std::optional<bool> Finalize() override {
			auto* ctx = GetOwner3DContext();
			if (!ctx) return false;

			if (auto* gd = ctx->GetComponent<GroundDefined>()) {
				gd->SetDefaultColor(originalDefaultColor_);
				gd->SetTouchColorStart(originalTouchColorStart_);
				gd->SetTouchColorEnd(originalTouchColorEnd_);
			}
			return true;
		};

		std::optional<bool> Update() override {
			auto* ctx = GetOwner3DContext();
			if (!ctx) return false;

			// 重力方向の目標角度を計算する
			float targetRotation = std::atan2(GravityDirection_.y, GravityDirection_.x);

			// 現在の角度から目標角度への最短差分を求める (-π 〜 +π に正規化)
			float diff = std::atan2(std::sin(targetRotation - currentRotation_), std::cos(targetRotation - currentRotation_));

			// 目標角度に向けて回転させる
			float step = rotationSpeed_ * GetDeltaTime();
			if (std::abs(diff) <= step) {
				rotateAcceleration_ *= 0.5f; // 目標に近づいたら減速
			} else {
				rotateAcceleration_ += (diff > 0.0f) ? step : -step; // 近い方向へ回る
			}
			currentRotation_ += rotateAcceleration_ * GetDeltaTime();

			if (auto* tr = ctx->GetComponent<Transform3D>()) {
				const Vector3 pos = tr->GetTranslate();
				Vector3 newPos =
					Vector3{ std::cos(currentRotation_) * rotationRadius_, std::sin(currentRotation_) * rotationRadius_, pos.z };
				tr->SetTranslate(newPos);
				tr->SetRotate(Vector3{ 0.0f, 0.0f, currentRotation_ + (3.141592f * 0.5f) });
			}
			return true;
		};

#if defined(USE_IMGUI)
		void ShowImGui() override {}
#endif

	private:
		const Vector4 defaultColor_{ 0.5f, 0.0f, 0.8f, 1.0f };
		const Vector4 touchColorStart_{ 1.0f, 1.0f, 1.0f, 1.0f };
		const Vector4 touchColorEnd_{ 1.0f, 0.5f, 0.5f, 1.0f };

		Vector4 originalDefaultColor_;
		Vector4 originalTouchColorStart_;
		Vector4 originalTouchColorEnd_;

		float rotationSpeed_ = 1.5f;
		float currentRotation_ = 0.0f;
		float rotationRadius_ = 0.0f;

		float rotateAcceleration_ = 0.0f;

		const Vector3& GravityDirection_;
	};
}