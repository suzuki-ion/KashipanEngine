#pragma once
#include "Objects/Components/Collider/ICollider.h"
#include "Math/Vector2.h"

namespace KashipanEngine {

/// @brief 2D用のカプセルコライダー
class Capsule2DCollider final : public ICollider {
public:
    Capsule2DCollider() : ICollider("Capsule2DCollider", Shape::Capsule2D, true, GetComponentTypeID<Capsule2DCollider>()) {}
    ~Capsule2DCollider() override = default;

    std::unique_ptr<IObjectComponent> Clone() const override {
        auto ptr = std::make_unique<Capsule2DCollider>();
        ptr->start_ = start_;
        ptr->end_ = end_;
        ptr->radius_ = radius_;
        ptr->SetTrigger(IsTrigger());
        return ptr;
    }

    void SetStart(const Vector2 &start) { start_ = start; }
    void SetEnd(const Vector2 &end) { end_ = end; }
    void SetRadius(float radius) { radius_ = radius; }
    const Vector2 &GetStart() const noexcept { return start_; }
    const Vector2 &GetEnd() const noexcept { return end_; }
    float GetRadius() const noexcept { return radius_; }

    std::optional<ColliderInfo2D> BuildColliderInfo2D() const override {
        ColliderInfo2D info;
        const Vector2 worldPos(GetOwnerWorldPosition());
        Math::Capsule2D capsule;
        capsule.start = worldPos + start_;
        capsule.end = worldPos + end_;
        capsule.radius = radius_;
        info.shape = capsule;
        info.ownerObject = GetOwnerObjectContext() ? const_cast<EmptyObject *>(GetOwnerObjectContext()->GetOwner()) : nullptr;
        return info;
    }

protected:
#if defined(USE_IMGUI)
    void ShowImGui() override {
        ICollider::ShowImGui();
        ImGui::DragFloat2("Start", &start_.x, 0.01f);
        ImGui::DragFloat2("End", &end_.x, 0.01f);
        ImGui::DragFloat("Radius", &radius_, 0.01f, 0.0f);
    }
#endif
    JSON SaveToJson() const override {
        JSON json = ICollider::SaveToJson();
        json["start"] = ToJSON(start_);
        json["end"] = ToJSON(end_);
        json["radius"] = radius_;
        return json;
    }
    bool LoadFromJson(const JSON &json) override {
        ICollider::LoadFromJson(json);
        if (json.contains("start")) start_ = FromJSON<Vector2>(json["start"]);
        if (json.contains("end")) end_ = FromJSON<Vector2>(json["end"]);
        radius_ = json.value("radius", 0.5f);
        return true;
    }

private:
    Vector2 start_{ 0.0f, -0.5f };
    Vector2 end_{ 0.0f, 0.5f };
    float radius_ = 0.5f;
};

REGISTER_COMPONENT_OBJECT(Capsule2DCollider)

} // namespace KashipanEngine
