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

        transform->SetScale(Vector3(MyEasing::Lerp(minScale_, maxScale_, bpmProgress_, easeType_)));

        return std::nullopt;
    }

#ifdef USE_IMGUI
    void BPMScaling::ShowImGui() {
        if (ImGui::TreeNode("BPMScaling")) { 
            ImGui::Text("BPM Progress: %.2f", bpmProgress_);

            ImGui::TreePop();
        }
    }
#endif
} // namespace KashipanEngine