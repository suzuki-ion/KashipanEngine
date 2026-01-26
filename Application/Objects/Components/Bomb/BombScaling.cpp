#include "BombScaling.h"
#include <cmath>

namespace KashipanEngine {

    std::optional<bool> BombScaling::Update() {
        auto* context = dynamic_cast<Object3DContext*>(GetOwnerContext());
        if (!context) {
            return std::nullopt;
        }

        auto* transform = context->GetComponent<Transform3D>();
        if (!transform) {
            return std::nullopt;
        }

        Vector3 targetScale;
       
        // 最終拍の場合は起爆スケールまで拡大
        if (elapsedBeats >= maxBeats - 1) {
            if (!speed2ScaleTimer_.IsActive()) {
                speed2ScaleTimer_.Start(0.1f, true);
                speedScaleTimer_.Reset();
            }
            if (speed2ScaleTimer_.IsFinished()) {
                countBeats_++;
            }

            if (countBeats_ >= 3) {
                if (!speed3ScaleTimer_.IsActive()) {
                    speed3ScaleTimer_.Start(0.2f, true);
                    speed2ScaleTimer_.Reset();
                }
                targetScale = MyEasing::Lerp(maxSpeedScale_, detonationScale_, speed3ScaleTimer_.GetProgress(), easeType_);
            } else {
                targetScale = MyEasing::Lerp(minSpeedScale_, maxSpeedScale_, speed2ScaleTimer_.GetProgress(), easeType_);
            }

        }
        // 最終拍の1つ前の場合は素早くスケーリング
        else if (elapsedBeats == maxBeats - 2) {
            if (!speedScaleTimer_.IsActive()) {
                speedScaleTimer_.Start(0.2f, true);
            }
            targetScale = MyEasing::Lerp(minSpeedScale_, maxSpeedScale_, speedScaleTimer_.GetProgress(), easeType_);
        }
        // 通常の拍
        else {
            targetScale = MyEasing::Lerp(minScale_, maxScale_, bpmProgress_, easeType_);
        }

        transform->SetScale(targetScale);

        speedScaleTimer_.Update();
        speed2ScaleTimer_.Update(); 
        speed3ScaleTimer_.Update();

        return std::nullopt;
    }

#ifdef USE_IMGUI
    void BombScaling::ShowImGui() {
        if (ImGui::TreeNode("BPMScaling")) {
            ImGui::Text("BPM Progress: %.2f", bpmProgress_);
            ImGui::Text("Elapsed Beats: %d / %d", elapsedBeats, maxBeats);

            ImGui::TreePop();
        }
    }
#endif
} // namespace KashipanEngine