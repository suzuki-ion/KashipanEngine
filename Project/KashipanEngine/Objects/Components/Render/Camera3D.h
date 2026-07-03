#pragma once
#include "Objects/ObjectComponentHeader.h"

namespace KashipanEngine {

class Camera3D final : public IObjectComponent {
public:
    OBJECT_COMPONENT_CONSTRUCTOR(Camera3D, 1, )
    ~Camera3D() override = default;
    std::unique_ptr<IObjectComponent> Clone() const override {
        auto ptr = std::make_unique<Camera3D>();
        ptr->fovY_ = fovY_;
        ptr->nearClip_ = nearClip_;
        ptr->farClip_ = farClip_;
        ptr->orthographic_ = orthographic_;
        return ptr;
    }
protected:
#if defined(USE_IMGUI)
    void ShowImGui() override { ImGui::Checkbox("Orthographic", &orthographic_); ImGui::DragFloat("FovY", &fovY_, 0.01f); ImGui::DragFloat("Near", &nearClip_, 0.01f); ImGui::DragFloat("Far", &farClip_, 1.0f); }
#endif
    JSON SaveToJson() const override { return JSON{ {"fovY", fovY_}, {"nearClip", nearClip_}, {"farClip", farClip_}, {"orthographic", orthographic_} }; }
    bool LoadFromJson(const JSON &json) override { fovY_ = json.value("fovY", 0.45f); nearClip_ = json.value("nearClip", 0.1f); farClip_ = json.value("farClip", 1000.0f); orthographic_ = json.value("orthographic", false); return true; }
private:
    float fovY_ = 0.45f;
    float nearClip_ = 0.1f;
    float farClip_ = 1000.0f;
    bool orthographic_ = false;
};

REGISTER_COMPONENT_OBJECT(Camera3D)

} // namespace KashipanEngine
