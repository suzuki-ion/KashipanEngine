#pragma once
#include "Objects/ObjectComponentHeader.h"
#include "Math/Vector2.h"

namespace KashipanEngine {

class RigidBody2D final : public IObjectComponent {
public:
    OBJECT_COMPONENT_CONSTRUCTOR(RigidBody2D, 1, )
    COMPONENT_CATEGORY("Collision")
    ~RigidBody2D() override = default;
    std::unique_ptr<IObjectComponent> Clone() const override {
        auto ptr = std::make_unique<RigidBody2D>();
        ptr->velocity_ = velocity_;
        ptr->mass_ = mass_;
        ptr->useGravity_ = useGravity_;
        return ptr;
    }
protected:
#if defined(USE_IMGUI)
    void ShowImGui() override { ImGui::DragFloat2("Velocity", &velocity_.x, 0.01f); ImGui::DragFloat("Mass", &mass_, 0.01f, 0.0f); ImGui::Checkbox("UseGravity", &useGravity_); }
#endif
    JSON SaveToJson() const override { return JSON{ {"velocity", ToJSON(velocity_)}, {"mass", mass_}, {"useGravity", useGravity_} }; }
    bool LoadFromJson(const JSON &json) override { if (json.contains("velocity")) velocity_ = FromJSON<Vector2>(json["velocity"]); mass_ = json.value("mass", 1.0f); useGravity_ = json.value("useGravity", true); return true; }
private:
    Vector2 velocity_{ 0.0f, 0.0f };
    float mass_ = 1.0f;
    bool useGravity_ = true;
};

REGISTER_COMPONENT_OBJECT(RigidBody2D)

} // namespace KashipanEngine
