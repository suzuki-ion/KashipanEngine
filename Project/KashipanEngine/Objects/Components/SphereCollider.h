#pragma once
#include "Objects/Components/ICollider.h"
#include "Math/Vector3.h"

namespace KashipanEngine {

class SphereCollider final : public ICollider {
public:
    SphereCollider() : ICollider("SphereCollider", Shape::Sphere, GetComponentTypeID<SphereCollider>()) {}
    ~SphereCollider() override = default;

    std::unique_ptr<IObjectComponent> Clone() const override {
        auto ptr = std::make_unique<SphereCollider>();
        ptr->radius_ = radius_;
        ptr->center_ = center_;
        ptr->SetTrigger(IsTrigger());
        return ptr;
    }

protected:
#if defined(USE_IMGUI)
    void ShowImGui() override {
        ICollider::ShowImGui();
        ImGui::DragFloat("Radius", &radius_, 0.01f, 0.0f);
        ImGui::DragFloat3("Center", &center_.x, 0.01f);
    }
#endif
    JSON SaveToJson() const override {
        JSON json = ICollider::SaveToJson();
        json["radius"] = radius_;
        json["center"] = ToJSON(center_);
        return json;
    }
    bool LoadFromJson(const JSON &json) override {
        ICollider::LoadFromJson(json);
        radius_ = json.value("radius", 0.5f);
        if (json.contains("center")) center_ = FromJSON<Vector3>(json["center"]);
        return true;
    }

private:
    float radius_ = 0.5f;
    Vector3 center_{ 0.0f, 0.0f, 0.0f };
};

REGISTER_COMPONENT_OBJECT(SphereCollider)

} // namespace KashipanEngine
