#pragma once
#include <KashipanEngine.h>
#include "Objects/Components/GroundDefined.h"

namespace KashipanEngine {
	class SlowGroundDefined final : public IObjectComponent3D {
	public:
		explicit SlowGroundDefined()
			: IObjectComponent3D("SlowGroundDefined", 1) {}
		~SlowGroundDefined() override = default;

		std::unique_ptr<IObjectComponent> Clone() const override {
			return std::make_unique<SlowGroundDefined>();
		}

		std::optional<bool> Initialize() override {
			auto* ctx = GetOwner3DContext();
			if (!ctx) return false;

			if (auto* gd = ctx->GetComponent<GroundDefined>()) {
				gd->SetDefaultColor(defaultColor_);
			}
			return true;
		};

		std::optional<bool> Update() override {
			return true;
		};

#if defined(USE_IMGUI)
		void ShowImGui() override {}
#endif

	private:
		const Vector4 defaultColor_{ 1.0f, 0.0f, 0.0f, 1.0f };
	};
}