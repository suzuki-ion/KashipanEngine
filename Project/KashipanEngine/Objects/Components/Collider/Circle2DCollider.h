#pragma once
#include <algorithm>
#include "Objects/Components/Collider/ICollider.h"
#include "Math/Vector2.h"

namespace KashipanEngine {

/// @brief 2D用の円コライダー
class Circle2DCollider final : public ICollider {
public:
    Circle2DCollider() : ICollider("Circle2DCollider", Shape::Circle2D, true, GetComponentTypeID<Circle2DCollider>()) {}
    ~Circle2DCollider() override = default;

    std::unique_ptr<IObjectComponent> Clone() const override {
        auto ptr = std::make_unique<Circle2DCollider>();
        ptr->radius_ = radius_;
        ptr->center_ = center_;
        ptr->SetTrigger(IsTrigger());
        ptr->CopySyncSettingsFrom(*this);
        return ptr;
    }

    void SetRadius(float radius) { radius_ = radius; }
    void SetCenter(const Vector2 &center) { center_ = center; }
    float GetRadius() const noexcept { return radius_; }
    const Vector2 &GetCenter() const noexcept { return center_; }

    std::optional<ColliderInfo2D> BuildColliderInfo2D() const override {
        ColliderInfo2D info;
        Math::Circle circle;
        const Vector3 scale = GetSyncedOwnerScale();
        circle.center = Vector2(GetSyncedOwnerPosition()) + RotateOffsetBySyncedRotation2D(center_);
        circle.radius = radius_ * std::max(scale.x, scale.y);
        info.shape = circle;
        info.ownerObject = GetOwnerObjectContext() ? const_cast<EmptyObject *>(GetOwnerObjectContext()->GetOwner()) : nullptr;
        return info;
    }

protected:
#if defined(USE_IMGUI)
    void ShowImGui() override {
        ICollider::ShowImGui();
        ImGui::DragFloat("Radius", &radius_, 0.01f, 0.0f);
        ImGui::DragFloat2("Center", &center_.x, 0.01f);
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
        if (json.contains("center")) center_ = FromJSON<Vector2>(json["center"]);
        return true;
    }

private:
    float radius_ = 0.5f;
    Vector2 center_{ 0.0f, 0.0f };
};

REGISTER_COMPONENT_OBJECT(Circle2DCollider)

} // namespace KashipanEngine
