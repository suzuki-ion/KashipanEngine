#pragma once
#include <algorithm>
#include "Objects/Components/Collider/ICollider.h"
#include "Math/Vector2.h"

namespace KashipanEngine {

/// @brief 2D用のレイ（線分）コライダー
/// @details オーナーオブジェクトのワールド座標を始点とし、指定した方向・長さの線分として扱われる
class Ray2DCollider final : public ICollider {
public:
    Ray2DCollider() : ICollider("Ray2DCollider", Shape::Ray2D, true, GetComponentTypeID<Ray2DCollider>()) {
        ADD_MEMBER_VARIABLE(direction_);
        ADD_MEMBER_VARIABLE(length_);
    }
    ~Ray2DCollider() override = default;

    std::unique_ptr<IObjectComponent> Clone() const override {
        auto ptr = std::make_unique<Ray2DCollider>();
        ptr->direction_ = direction_;
        ptr->length_ = length_;
        ptr->SetTrigger(IsTrigger());
        ptr->CopySyncSettingsFrom(*this);
        return ptr;
    }

    void SetDirection(const Vector2 &direction) { direction_ = direction; }
    void SetLength(float length) { length_ = length; }
    const Vector2 &GetDirection() const noexcept { return direction_; }
    float GetLength() const noexcept { return length_; }

    std::optional<ColliderInfo2D> BuildColliderInfo2D() const override {
        ColliderInfo2D info;
        Math::Segment2D segment;
        const Vector3 scale = GetSyncedOwnerScale();
        segment.start = Vector2(GetSyncedOwnerPosition());
        const Vector2 rotatedDir = RotateOffsetBySyncedRotation2D(direction_.Normalize());
        segment.end = segment.start + rotatedDir * (length_ * std::max(scale.x, scale.y));
        info.shape = segment;
        info.ownerObject = GetOwnerObjectContext() ? const_cast<EmptyObject *>(GetOwnerObjectContext()->GetOwner()) : nullptr;
        return info;
    }

protected:
#if defined(USE_IMGUI)
    void ShowImGui() override {
        ICollider::ShowImGui();
        ImGui::DragFloat2("Direction", &direction_.x, 0.01f);
        ImGui::DragFloat("Length", &length_, 0.01f, 0.0f);
    }
#endif
    JSON SaveToJson() const override {
        JSON json = ICollider::SaveToJson();
        json["direction"] = ToJSON(direction_);
        json["length"] = length_;
        return json;
    }
    bool LoadFromJson(const JSON &json) override {
        ICollider::LoadFromJson(json);
        if (json.contains("direction")) direction_ = FromJSON<Vector2>(json["direction"]);
        length_ = json.value("length", 1.0f);
        return true;
    }

private:
    Vector2 direction_{ 0.0f, -1.0f };
    float length_ = 1.0f;
};

REGISTER_COMPONENT_OBJECT(Ray2DCollider)

} // namespace KashipanEngine
