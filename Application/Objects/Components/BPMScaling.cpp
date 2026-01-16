#include "BPMScaling.h"
#include <cmath>

namespace KashipanEngine {

    std::optional<bool> BPMScaling::Update() {
        auto* context = dynamic_cast<Object3DContext*>(GetOwnerContext());
        if (!context) {
            return std::nullopt;
        }

        auto* transform = context->GetComponent<Transform3D>();
        if (!transform) {
            return std::nullopt;
        }

        transform->SetScale(Vector3(Lerp(minScale_, maxScale_, bpmProgress_)));

        return std::nullopt;
    }

    void BPMScaling::ShowImGui() {
#ifdef USE_IMGUI
        if (ImGui::TreeNode("BPMScaling")) {
            ImGui::SliderFloat("Min Scale", &minScale_, 0.5f, 1.0f);
            ImGui::SliderFloat("Max Scale", &maxScale_, 1.0f, 2.0f);
            ImGui::Text("BPM Progress: %.2f", bpmProgress_);

            ImGui::TreePop();
        }
#endif
    }
} // namespace KashipanEngine