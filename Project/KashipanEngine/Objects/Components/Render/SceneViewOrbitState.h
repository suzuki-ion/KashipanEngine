#pragma once
#include "Debug/Logger.h"
#include "Objects/ObjectComponentHeader.h"
#include "Utilities/Translation.h"

namespace KashipanEngine {

/// @brief シーンビューのオービットカメラ操作用パラメータ（注視点までの距離）を保持するだけのコンポーネント
/// @details カメラの位置・向き（yaw/pitch/target相当）はTransformの位置・回転から復元できるが、
///          距離だけはTransformに情報が残らないため、シーンビュー用の「Scene View」オブジェクトに
///          このコンポーネントを付与して保持する（SceneEditorView::EnsureSceneViewObject参照）
class SceneViewOrbitState final : public IObjectComponent {
public:
    OBJECT_COMPONENT_CONSTRUCTOR(SceneViewOrbitState, 1,
        ADD_MEMBER_VARIABLE(distance_);
    )
    COMPONENT_CATEGORY("Render")
    ~SceneViewOrbitState() override = default;

    std::unique_ptr<IObjectComponent> Clone() const override {
        LogScope scope;
        auto ptr = std::make_unique<SceneViewOrbitState>();
        ptr->distance_ = distance_;
        return ptr;
    }

    void SetDistance(float distance) { distance_ = distance; }
    float GetDistance() const noexcept { return distance_; }

protected:
#if defined(USE_IMGUI)
    void ShowImGui() override {
        LogScope scope;
        ImGui::DragFloat(TranslationLabel("component.scenevieworbitstate.distance"), &distance_, 0.1f, 0.1f, 10000.0f);
    }
#endif

    JSON SaveToJson() const override {
        LogScope scope;
        return JSON{ {"distance", distance_} };
    }
    bool LoadFromJson(const JSON &json) override {
        LogScope scope;
        distance_ = json.value("distance", 10.0f);
        return true;
    }

private:
    float distance_ = 10.0f;
};

REGISTER_COMPONENT_OBJECT(SceneViewOrbitState)

} // namespace KashipanEngine
