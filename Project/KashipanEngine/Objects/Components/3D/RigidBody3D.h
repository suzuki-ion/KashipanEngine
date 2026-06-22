#pragma once
#include "Objects/ObjectComponentHeader.h"
#include "Objects/Components/3D/Transform3D.h"
#include "Math/Vector3.h"
#include "Math/Quaternion.h"
#include "Scene/Components/ColliderComponent.h"
#include <reactphysics3d/reactphysics3d.h>
#include <memory>
#include <optional>

namespace KashipanEngine {

class RigidBody3D final : public IObjectComponent3D {
public:
    RigidBody3D() : IObjectComponent3D("RigidBody3D", 1) {}

    ~RigidBody3D() override = default;

    std::unique_ptr<IObjectComponent> Clone() const override {
        auto ptr = std::make_unique<RigidBody3D>();
        ptr->world_ = world_;
        ptr->bodyType_ = bodyType_;
        ptr->mass_ = mass_;
        ptr->useGravity_ = useGravity_;
        ptr->interpolate_ = interpolate_;
        ptr->isInitialized_ = false;
        return ptr;
    }

    std::optional<bool> Initialize() override {
        return TryInitialize();
    }

    std::optional<bool> Finalize() override {
        return true;
    }

    std::optional<bool> Update() override {
        TryInitialize();
        if (!rigidBody_) return true;
        auto *ctx = GetOwner3DContext();
        if (!ctx) return false;
        auto *tr = ctx->GetComponent<Transform3D>();
        if (!tr) return false;

        const auto transform = rigidBody_->getTransform();
        const auto pos = transform.getPosition();
        const auto rot = transform.getOrientation();

        tr->SetTranslate(Vector3{pos.x, pos.y, pos.z});
        tr->SetRotateQuaternion(Quaternion{rot.x, rot.y, rot.z, rot.w});
        return true;
    }

    void SetPhysicsWorld(reactphysics3d::PhysicsWorld *world) { world_ = world; }

    reactphysics3d::RigidBody *GetRigidBody() const { return rigidBody_; }

    void SetBodyType(reactphysics3d::BodyType type) {
        bodyType_ = type;
        if (rigidBody_) rigidBody_->setType(type);
    }

    reactphysics3d::BodyType GetBodyType() const { return bodyType_; }

    void SetMass(float mass) {
        mass_ = mass;
        if (rigidBody_) rigidBody_->setMass(mass);
    }

    float GetMass() const { return mass_; }

    void SetUseGravity(bool enabled) {
        useGravity_ = enabled;
        if (rigidBody_) rigidBody_->enableGravity(enabled);
    }

    bool IsGravityEnabled() const { return useGravity_; }

    void SetInterpolate(bool enabled) {
        interpolate_ = enabled;
        if (rigidBody_) rigidBody_->setIsSleeping(!enabled);
    }

    bool IsInterpolateEnabled() const { return interpolate_; }

#ifdef USE_IMGUI
    void ShowImGui() override {
        ImGui::Text("RigidBody3D Component");
        if (ImGui::CollapsingHeader("Settings")) {
            if (ImGui::Combo("Body Type", reinterpret_cast<int *>(&bodyType_), "Static\0Kinematic\0Dynamic\0")) {
                SetBodyType(bodyType_);
            }
            if (ImGui::SliderFloat("Mass", &mass_, 0.1f, 100.0f)) {
                SetMass(mass_);
            }
            if (ImGui::Checkbox("Use Gravity", &useGravity_)) {
                SetUseGravity(useGravity_);
            }
            if (ImGui::Checkbox("Interpolate", &interpolate_)) {
                SetInterpolate(interpolate_);
            }
        }
    }
#endif

private:
    bool TryInitialize() {
        if (isInitialized_) return true;
        auto *sceneCtx = GetOwnerSceneContext();
        auto *colliderComp = sceneCtx ? sceneCtx->GetComponent<ColliderComponent>() : nullptr;
        if (!sceneCtx || !colliderComp) return false;
        world_ = sceneCtx->GetComponent<ColliderComponent>()->GetCollider()->GetPhysicsWorld();
        if (!world_) return false;
        auto *ctx = GetOwner3DContext();
        if (!ctx) return false;
        auto *tr = ctx->GetComponent<Transform3D>();
        if (!tr) return false;

        const Vector3 pos = tr->GetTranslate();
        const Quaternion rot = tr->GetRotateQuaternion();
        reactphysics3d::Transform transform(
            reactphysics3d::Vector3(pos.x, pos.y, pos.z),
            reactphysics3d::Quaternion(rot.x, rot.y, rot.z, rot.w));

        rigidBody_ = world_->createRigidBody(transform);
        if (!rigidBody_) return false;

        ApplySettings();
        isInitialized_ = true;
        return true;
    }
    void ApplySettings() {
        if (!rigidBody_) return;
        rigidBody_->setType(bodyType_);
        rigidBody_->setMass(mass_);
        rigidBody_->enableGravity(useGravity_);
        rigidBody_->setIsSleeping(!interpolate_);
    }

    reactphysics3d::PhysicsWorld *world_ = nullptr;
    reactphysics3d::RigidBody *rigidBody_ = nullptr;
    reactphysics3d::BodyType bodyType_ = reactphysics3d::BodyType::DYNAMIC;
    float mass_ = 1.0f;
    bool useGravity_ = true;
    bool interpolate_ = true;

    bool isInitialized_ = false;
};

REGISTER_COMPONENT_OBJECT3D(RigidBody3D)

} // namespace KashipanEngine
