#pragma once
#include <KashipanEngine.h>
#include "Objects/Components/GroundDefined.h"

namespace KashipanEngine {
	class RotateGroundDefined final : public IObjectComponent3D {
	public:
		explicit RotateGroundDefined()
			: IObjectComponent3D("RotateGroundDefined", 1) {
		}
		~RotateGroundDefined() override = default;

		std::unique_ptr<IObjectComponent> Clone() const override {
			return std::make_unique<RotateGroundDefined>();
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

			currentRotation_ += rotationSpeed_ * GetDeltaTime();

			if (auto* tr = ctx->GetComponent<Transform3D>()) {
				const Vector3 pos = tr->GetTranslate();
				Vector3 newPos = 
					Vector3{ std::cos(currentRotation_) * rotationRadius_, std::sin(currentRotation_) * rotationRadius_, pos.z };
				tr->SetTranslate(newPos);
				tr->SetRotate(Vector3{ 0.0f, 0.0f, currentRotation_ + (3.14f * 0.5f) });
			}
			return true;
		};

#if defined(USE_IMGUI)
		void ShowImGui() override {}
#endif

	private:
		const Vector4 defaultColor_{ 0.0f, 0.6f, 0.8f, 1.0f };
		const Vector4 touchColorStart_{ 1.0f, 1.0f, 1.0f, 1.0f };
		const Vector4 touchColorEnd_{ 1.0f, 0.5f, 0.5f, 1.0f };

		Vector4 originalDefaultColor_;
		Vector4 originalTouchColorStart_;
		Vector4 originalTouchColorEnd_;

		float rotationSpeed_ = 0.5f;
		float currentRotation_ = 0.0f;
		float rotationRadius_ = 0.0f;
	};
}