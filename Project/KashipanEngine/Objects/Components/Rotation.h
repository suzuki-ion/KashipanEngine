#pragma once
#include "Objects/ObjectComponentHeader.h"
#include "Objects/Components/Transform.h"
#include "Math/Vector3.h"
#include "Utilities/TimeUtils.h"

namespace KashipanEngine {

/// @brief Transform に対して回転の更新を行うコンポーネント
/// @details 毎フレーム、角加速度を角速度へ、角速度を Transform の回転（オイラー角、ラジアン）へ適用する。
class Rotation final : public IObjectComponent {
public:
    OBJECT_COMPONENT_CONSTRUCTOR(Rotation, 1,
        ADD_MEMBER_VARIABLE(angularVelocity_);
        ADD_MEMBER_VARIABLE(angularAcceleration_);
    )
    COMPONENT_CATEGORY("Physics")
    ~Rotation() override = default;

    std::unique_ptr<IObjectComponent> Clone() const override {
        auto ptr = std::make_unique<Rotation>();
        ptr->angularVelocity_ = angularVelocity_;
        ptr->angularAcceleration_ = angularAcceleration_;
        return ptr;
    }

    void SetAngularVelocity(const Vector3 &angularVelocity) { angularVelocity_ = angularVelocity; }
    const Vector3 &GetAngularVelocity() const noexcept { return angularVelocity_; }
    void SetAngularAcceleration(const Vector3 &angularAcceleration) { angularAcceleration_ = angularAcceleration; }
    const Vector3 &GetAngularAcceleration() const noexcept { return angularAcceleration_; }

    /// @brief 角速度に加算する
    void AddAngularVelocity(const Vector3 &angularVelocity) { angularVelocity_ = angularVelocity_ + angularVelocity; }

protected:
    void Update() override {
        auto *objectContext = GetOwnerObjectContext();
        if (!objectContext) return;
        auto *transform = objectContext->GetComponent<Transform>();
        if (!transform) return;

        const float deltaTime = GetDeltaTime();
        angularVelocity_ = angularVelocity_ + angularAcceleration_ * deltaTime;
        transform->SetRotate(transform->GetRotate() + angularVelocity_ * deltaTime);
    }

#if defined(USE_IMGUI)
    void ShowImGui() override {
        ImGui::DragFloat3("Angular Velocity", &angularVelocity_.x, 0.01f);
        ImGui::DragFloat3("Angular Acceleration", &angularAcceleration_.x, 0.01f);
    }
#endif

    JSON SaveToJson() const override {
        JSON json = JSON::object();
        json["angularVelocity"] = ToJSON(angularVelocity_);
        json["angularAcceleration"] = ToJSON(angularAcceleration_);
        return json;
    }

    bool LoadFromJson(const JSON &json) override {
        if (json.contains("angularVelocity")) angularVelocity_ = FromJSON<Vector3>(json["angularVelocity"]);
        if (json.contains("angularAcceleration")) angularAcceleration_ = FromJSON<Vector3>(json["angularAcceleration"]);
        return true;
    }

private:
    /// @brief 角速度（ラジアン/秒）
    Vector3 angularVelocity_{ 0.0f, 0.0f, 0.0f };
    /// @brief 角加速度（ラジアン/秒^2）
    Vector3 angularAcceleration_{ 0.0f, 0.0f, 0.0f };
};

REGISTER_COMPONENT_OBJECT(Rotation)

} // namespace KashipanEngine
