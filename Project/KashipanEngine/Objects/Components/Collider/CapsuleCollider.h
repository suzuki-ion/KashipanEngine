#pragma once
#include "Objects/Components/Collider/ICollider.h"
#include "Math/Vector3.h"

namespace KashipanEngine {

class CapsuleCollider final : public ICollider {
public:
    CapsuleCollider() : ICollider("CapsuleCollider", Shape::Capsule, GetComponentTypeID<CapsuleCollider>()) {}
    ~CapsuleCollider() override = default;

    std::unique_ptr<IObjectComponent> Clone() const override {
        auto ptr = std::make_unique<CapsuleCollider>();
        ptr->radius_ = radius_;
        ptr->height_ = height_;
        ptr->center_ = center_;
        ptr->SetTrigger(IsTrigger());
        return ptr;
    }

protected:
#if defined(USE_IMGUI)
    void ShowImGui() override {
        ICollider::ShowImGui();
        ImGui::DragFloat("Radius", &radius_, 0.01f, 0.0f);
        ImGui::DragFloat("Height", &height_, 0.01f, 0.0f);
        ImGui::DragFloat3("Center", &center_.x, 0.01f);
    }
#endif
    JSON SaveToJson() const override {
        JSON json = ICollider::SaveToJson();
        json["radius"] = radius_;
        json["height"] = height_;
        json["center"] = ToJSON(center_);
        return json;
    }
    bool LoadFromJson(const JSON &json) override {
        ICollider::LoadFromJson(json);
        radius_ = json.value("radius", 0.5f);
        height_ = json.value("height", 1.0f);
        if (json.contains("center")) center_ = FromJSON<Vector3>(json["center"]);
        return true;
    }

private:
    float radius_ = 0.5f;
    float height_ = 1.0f;
    Vector3 center_{ 0.0f, 0.0f, 0.0f };
};

REGISTER_COMPONENT_OBJECT(CapsuleCollider)

} // namespace KashipanEngine
