#pragma once
#include <algorithm>
#include "Objects/Components/Collider/ICollider.h"
#include "Math/Vector2.h"
#include "Utilities/Translation.h"

namespace KashipanEngine {

/// @brief 2D用のカプセルコライダー
class Capsule2DCollider final : public ICollider {
public:
    Capsule2DCollider() : ICollider("Capsule2DCollider", Shape::Capsule2D, true, GetComponentTypeID<Capsule2DCollider>()) {
        ADD_MEMBER_VARIABLE(start_);
        ADD_MEMBER_VARIABLE(end_);
        ADD_MEMBER_VARIABLE(radius_);
    }
    ~Capsule2DCollider() override = default;

    std::unique_ptr<IObjectComponent> Clone() const override {
        auto ptr = std::make_unique<Capsule2DCollider>();
        ptr->start_ = start_;
        ptr->end_ = end_;
        ptr->radius_ = radius_;
        ptr->SetTrigger(IsTrigger());
        ptr->CopySyncSettingsFrom(*this);
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
        const Vector2 worldPos(GetSyncedOwnerPosition());
        const Vector3 scale = GetSyncedOwnerScale();
        Math::Capsule2D capsule;
        capsule.start = worldPos + RotateOffsetBySyncedRotation2D(Vector2(start_.x * scale.x, start_.y * scale.y));
        capsule.end = worldPos + RotateOffsetBySyncedRotation2D(Vector2(end_.x * scale.x, end_.y * scale.y));
        capsule.radius = radius_ * std::max(scale.x, scale.y);
        info.shape = capsule;
        info.ownerObject = GetOwnerObjectContext() ? const_cast<EmptyObject *>(GetOwnerObjectContext()->GetOwner()) : nullptr;
        return info;
    }

protected:
#if defined(USE_IMGUI)
    void ShowImGui() override {
        ICollider::ShowImGui();
        ImGui::DragFloat2(TranslationLabel("component.capsule2dcollider.start"), &start_.x, 0.01f);
        ImGui::DragFloat2(TranslationLabel("component.capsule2dcollider.end"), &end_.x, 0.01f);
        ImGui::DragFloat(TranslationLabel("component.capsule2dcollider.radius"), &radius_, 0.01f, 0.0f);
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
