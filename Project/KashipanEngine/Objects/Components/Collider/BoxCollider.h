#pragma once
#include "Debug/Logger.h"
#include "Objects/Components/Collider/ICollider.h"
#include "Math/Vector3.h"
#include "Utilities/Translation.h"

namespace KashipanEngine {

class BoxCollider final : public ICollider {
public:
    BoxCollider() : ICollider("BoxCollider", Shape::Box, false, GetComponentTypeID<BoxCollider>()) {
        LogScope scope;
        ADD_MEMBER_VARIABLE(size_);
        ADD_MEMBER_VARIABLE(center_);
    }
    ~BoxCollider() override = default;

    std::unique_ptr<IObjectComponent> Clone() const override {
        LogScope scope;
        auto ptr = std::make_unique<BoxCollider>();
        ptr->size_ = size_;
        ptr->center_ = center_;
        ptr->SetTrigger(IsTrigger());
        ptr->CopySyncSettingsFrom(*this);
        return ptr;
    }

    void SetSize(const Vector3 &size) { size_ = size; }
    void SetCenter(const Vector3 &center) { center_ = center; }
    const Vector3 &GetSize() const noexcept { return size_; }
    const Vector3 &GetCenter() const noexcept { return center_; }

    std::optional<ColliderInfo3D> BuildColliderInfo3D() const override {
        LogScope scope;
        ColliderInfo3D info;
        ColliderInfo3D::BoxShape3D box;
        const Vector3 scale = GetSyncedOwnerScale();
        box.center = GetSyncedOwnerPosition() + center_;
        box.halfExtents = Vector3(size_.x * scale.x, size_.y * scale.y, size_.z * scale.z) * 0.5f;
        info.shape = box;
        info.ownerObject = const_cast<EmptyObject *>(GetOwnerObjectContext() ? GetOwnerObjectContext()->GetOwner() : nullptr);
        return info;
    }

protected:
#if defined(USE_IMGUI)
    void ShowImGui() override {
        LogScope scope;
        ICollider::ShowImGui();
        ImGui::DragFloat3(TranslationLabel("component.boxcollider.size"), &size_.x, 0.01f);
        ImGui::DragFloat3(TranslationLabel("component.boxcollider.center"), &center_.x, 0.01f);
    }
#endif
    JSON SaveToJson() const override {
        LogScope scope;
        JSON json = ICollider::SaveToJson();
        json["size"] = ToJSON(size_);
        json["center"] = ToJSON(center_);
        return json;
    }
    bool LoadFromJson(const JSON &json) override {
        LogScope scope;
        ICollider::LoadFromJson(json);
        if (json.contains("size")) size_ = FromJSON<Vector3>(json["size"]);
        if (json.contains("center")) center_ = FromJSON<Vector3>(json["center"]);
        return true;
    }

private:
    Vector3 size_{ 1.0f, 1.0f, 1.0f };
    Vector3 center_{ 0.0f, 0.0f, 0.0f };
};

REGISTER_COMPONENT_OBJECT(BoxCollider)

} // namespace KashipanEngine
