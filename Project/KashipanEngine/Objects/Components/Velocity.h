#pragma once
#include "Objects/ObjectComponentHeader.h"
#include "Objects/Components/Transform.h"
#include "Math/Vector3.h"
#include "Utilities/TimeUtils.h"

namespace KashipanEngine {

/// @brief Transform に対して移動の更新を行うコンポーネント
/// @details 毎フレーム、加速度を速度へ、速度を Transform の座標へ適用する。
class Velocity final : public IObjectComponent {
public:
    OBJECT_COMPONENT_CONSTRUCTOR(Velocity, 1,
        ADD_MEMBER_VARIABLE(velocity_);
        ADD_MEMBER_VARIABLE(acceleration_);
    )
    COMPONENT_CATEGORY("Physics")
    ~Velocity() override = default;

    std::unique_ptr<IObjectComponent> Clone() const override {
        auto ptr = std::make_unique<Velocity>();
        ptr->velocity_ = velocity_;
        ptr->acceleration_ = acceleration_;
        return ptr;
    }

    void SetVelocity(const Vector3 &velocity) { velocity_ = velocity; }
    const Vector3 &GetVelocity() const noexcept { return velocity_; }
    void SetAcceleration(const Vector3 &acceleration) { acceleration_ = acceleration; }
    const Vector3 &GetAcceleration() const noexcept { return acceleration_; }

    /// @brief 速度に加算する
    void AddVelocity(const Vector3 &velocity) { velocity_ = velocity_ + velocity; }

protected:
    void Update() override {
        auto *objectContext = GetOwnerObjectContext();
        if (!objectContext) return;
        auto *transform = objectContext->GetComponent<Transform>();
        if (!transform) return;

        const float deltaTime = GetDeltaTime();
        velocity_ = velocity_ + acceleration_ * deltaTime;
        transform->SetTranslate(transform->GetTranslate() + velocity_ * deltaTime);
    }

#if defined(USE_IMGUI)
    void ShowImGui() override {
        ImGui::DragFloat3("Velocity", &velocity_.x, 0.01f);
        ImGui::DragFloat3("Acceleration", &acceleration_.x, 0.01f);
    }
#endif

    JSON SaveToJson() const override {
        JSON json = JSON::object();
        json["velocity"] = ToJSON(velocity_);
        json["acceleration"] = ToJSON(acceleration_);
        return json;
    }

    bool LoadFromJson(const JSON &json) override {
        if (json.contains("velocity")) velocity_ = FromJSON<Vector3>(json["velocity"]);
        if (json.contains("acceleration")) acceleration_ = FromJSON<Vector3>(json["acceleration"]);
        return true;
    }

private:
    /// @brief 速度（単位/秒）
    Vector3 velocity_{ 0.0f, 0.0f, 0.0f };
    /// @brief 加速度（単位/秒^2）
    Vector3 acceleration_{ 0.0f, 0.0f, 0.0f };
};

REGISTER_COMPONENT_OBJECT(Velocity)

} // namespace KashipanEngine
