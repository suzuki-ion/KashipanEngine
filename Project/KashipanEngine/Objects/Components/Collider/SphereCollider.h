#pragma once
#include <algorithm>
#include "Objects/Components/Collider/ICollider.h"
#include "Math/Vector3.h"

namespace KashipanEngine {

class SphereCollider final : public ICollider {
public:
    SphereCollider() : ICollider("SphereCollider", Shape::Sphere, false, GetComponentTypeID<SphereCollider>()) {
        ADD_MEMBER_VARIABLE(radius_);
        ADD_MEMBER_VARIABLE(center_);
    }
    ~SphereCollider() override = default;

    std::unique_ptr<IObjectComponent> Clone() const override {
        auto ptr = std::make_unique<SphereCollider>();
        ptr->radius_ = radius_;
        ptr->center_ = center_;
        ptr->SetTrigger(IsTrigger());
        ptr->CopySyncSettingsFrom(*this);
        return ptr;
    }

    void SetRadius(float radius) { radius_ = radius; }
    void SetCenter(const Vector3 &center) { center_ = center; }
    float GetRadius() const noexcept { return radius_; }
    const Vector3 &GetCenter() const noexcept { return center_; }

    std::optional<ColliderInfo3D> BuildColliderInfo3D() const override {
        ColliderInfo3D info;
        ColliderInfo3D::SphereShape3D sphere;
        const Vector3 scale = GetSyncedOwnerScale();
        sphere.center = GetSyncedOwnerPosition() + center_;
        sphere.radius = radius_ * std::max({ scale.x, scale.y, scale.z });
        info.shape = sphere;
        info.ownerObject = const_cast<EmptyObject *>(GetOwnerObjectContext() ? GetOwnerObjectContext()->GetOwner() : nullptr);
        return info;
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
