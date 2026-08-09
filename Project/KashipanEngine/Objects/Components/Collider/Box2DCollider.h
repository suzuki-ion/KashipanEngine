#pragma once
#include "Objects/Components/Collider/ICollider.h"
#include "Math/Vector2.h"
#include "Utilities/Translation.h"

namespace KashipanEngine {

/// @brief 2D用の矩形コライダー
class Box2DCollider final : public ICollider {
public:
    Box2DCollider() : ICollider("Box2DCollider", Shape::Box2D, true, GetComponentTypeID<Box2DCollider>()) {
        ADD_MEMBER_VARIABLE(size_);
        ADD_MEMBER_VARIABLE(center_);
    }
    ~Box2DCollider() override = default;

    std::unique_ptr<IObjectComponent> Clone() const override {
        auto ptr = std::make_unique<Box2DCollider>();
        ptr->size_ = size_;
        ptr->center_ = center_;
        ptr->SetTrigger(IsTrigger());
        ptr->CopySyncSettingsFrom(*this);
        return ptr;
    }

    void SetSize(const Vector2 &size) { size_ = size; }
    void SetCenter(const Vector2 &center) { center_ = center; }
    const Vector2 &GetSize() const noexcept { return size_; }
    const Vector2 &GetCenter() const noexcept { return center_; }

    std::optional<ColliderInfo2D> BuildColliderInfo2D() const override {
        ColliderInfo2D info;
        Math::Rect rect;
        const Vector3 scale = GetSyncedOwnerScale();
        rect.center = Vector2(GetSyncedOwnerPosition()) + RotateOffsetBySyncedRotation2D(center_);
        rect.halfSize = Vector2(size_.x * scale.x, size_.y * scale.y) * 0.5f;
        rect.rotation = GetSyncedOwnerRotationEuler().z;
        info.shape = rect;
        info.ownerObject = GetOwnerObjectContext() ? const_cast<EmptyObject *>(GetOwnerObjectContext()->GetOwner()) : nullptr;
        return info;
    }

protected:
#if defined(USE_IMGUI)
    void ShowImGui() override {
        ICollider::ShowImGui();
        ImGui::DragFloat2(TranslationLabel("component.box2dcollider.size"), &size_.x, 0.01f);
        ImGui::DragFloat2(TranslationLabel("component.box2dcollider.center"), &center_.x, 0.01f);
    }
#endif
    JSON SaveToJson() const override {
        JSON json = ICollider::SaveToJson();
        json["size"] = ToJSON(size_);
        json["center"] = ToJSON(center_);
        return json;
    }
    bool LoadFromJson(const JSON &json) override {
        ICollider::LoadFromJson(json);
        if (json.contains("size")) size_ = FromJSON<Vector2>(json["size"]);
        if (json.contains("center")) center_ = FromJSON<Vector2>(json["center"]);
        return true;
    }

private:
    Vector2 size_{ 1.0f, 1.0f };
    Vector2 center_{ 0.0f, 0.0f };
};

REGISTER_COMPONENT_OBJECT(Box2DCollider)

} // namespace KashipanEngine
