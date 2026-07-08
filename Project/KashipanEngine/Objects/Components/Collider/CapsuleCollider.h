#pragma once
#include "Objects/Components/Collider/ICollider.h"
#include "Math/Vector3.h"

namespace KashipanEngine {

class CapsuleCollider final : public ICollider {
public:
    CapsuleCollider() : ICollider("CapsuleCollider", Shape::Capsule, false, GetComponentTypeID<CapsuleCollider>()) {}
    ~CapsuleCollider() override = default;

    std::unique_ptr<IObjectComponent> Clone() const override {
        auto ptr = std::make_unique<CapsuleCollider>();
        ptr->radius_ = radius_;
        ptr->height_ = height_;
        ptr->center_ = center_;
        ptr->SetTrigger(IsTrigger());
        return ptr;
    }

    void SetRadius(float radius) { radius_ = radius; }
    void SetHeight(float height) { height_ = height; }
    void SetCenter(const Vector3 &center) { center_ = center; }
    float GetRadius() const noexcept { return radius_; }
    float GetHeight() const noexcept { return height_; }
    const Vector3 &GetCenter() const noexcept { return center_; }

    std::optional<ColliderInfo3D> BuildColliderInfo3D() const override {
        ColliderInfo3D info;
        ColliderInfo3D::CapsuleShape3D capsule;
        capsule.center = GetOwnerWorldPosition() + center_;
        capsule.radius = radius_;
        capsule.height = height_;
        info.shape = capsule;
        info.ownerObject = const_cast<EmptyObject *>(GetOwnerObjectContext() ? GetOwnerObjectContext()->GetOwner() : nullptr);
        return info;
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
