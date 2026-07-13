#pragma once
#include "Objects/ObjectComponentHeader.h"
#include "Objects/Components/Collider/ICollider.h"
#include "Objects/Components/Transform.h"
#include "Math/Vector3.h"
#include "Math/Quaternion.h"
#include "Scene/Components/SceneObjectCollider.h"
#include <reactphysics3d/reactphysics3d.h>
#include <memory>
#include <vector>

namespace KashipanEngine {

class RigidBody3D final : public IObjectComponent {
public:
    OBJECT_COMPONENT_CONSTRUCTOR(RigidBody3D, 1, )
    COMPONENT_CATEGORY("Collision")
    ~RigidBody3D() override = default;

    std::unique_ptr<IObjectComponent> Clone() const override {
        auto ptr = std::make_unique<RigidBody3D>();
        ptr->world_ = world_;
        ptr->bodyType_ = bodyType_;
        ptr->mass_ = mass_;
        ptr->useGravity_ = useGravity_;
        ptr->interpolate_ = interpolate_;
        ptr->isInitialized_ = false;
        ptr->selectedColliderTypeName_ = selectedColliderTypeName_;
        ptr->selectedColliderOccurrenceIndex_ = selectedColliderOccurrenceIndex_;
        return ptr;
    }

    //==================================================
    // 使用するColliderコンポーネントの選択
    //==================================================

    /// @brief 同一オブジェクト上のICollider派生コンポーネントから使用する形状を選択する
    /// @param collider 選択するコライダー（nullptrの場合は未選択＝どのコライダーでも使用可）
    void SetSelectedCollider(ICollider *collider) {
        if (!collider) {
            selectedColliderTypeName_.clear();
            selectedColliderOccurrenceIndex_ = 0;
            return;
        }
        int occurrence = 0;
        for (auto *candidate : GetOwnerColliders()) {
            if (candidate == collider) {
                selectedColliderTypeName_ = candidate->GetComponentType();
                selectedColliderOccurrenceIndex_ = occurrence;
                return;
            }
            if (candidate->GetComponentType() == collider->GetComponentType()) ++occurrence;
        }
    }
    /// @brief 選択中のコライダーを取得（未選択・見つからない場合は nullptr）
    ICollider *GetSelectedCollider() const {
        if (selectedColliderTypeName_.empty()) return nullptr;
        int occurrence = 0;
        for (auto *candidate : GetOwnerColliders()) {
            if (candidate->GetComponentType() != selectedColliderTypeName_) continue;
            if (occurrence == selectedColliderOccurrenceIndex_) return candidate;
            ++occurrence;
        }
        return nullptr;
    }
    /// @brief 同一オブジェクト上の全ICollider派生コンポーネントを取得
    std::vector<ICollider *> GetOwnerColliders() const {
        std::vector<ICollider *> result;
        auto *ctx = GetOwnerObjectContext();
        if (!ctx) return result;
        for (const auto &pair : ctx->GetAllComponents()) {
            if (auto *collider = dynamic_cast<ICollider *>(pair.first.get())) {
                result.push_back(collider);
            }
        }
        return result;
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

    /// @brief 現在のTransformの位置・回転を物理ボディへ反映する
    /// @details エディターでオブジェクトを移動させても、既に生成済みの物理ボディの位置は
    ///          自動的には追従しない（毎フレームUpdateで行っているのは物理→Transformへの反映のみ）。
    ///          そのままPlayを開始すると、物理ボディが生成された時点の古い位置（多くの場合原点）へ
    ///          Transformが引き戻されてしまうため、Play開始時にこれを呼んで同期を取る。
    void SyncFromTransform() {
        if (!rigidBody_) return;
        auto *ctx = GetOwnerObjectContext();
        auto *tr = ctx ? ctx->GetComponent<Transform>() : nullptr;
        if (!tr) return;

        const Vector3 pos = tr->GetTranslate();
        const Quaternion rot = tr->GetRotateQuaternion();
        rigidBody_->setTransform(reactphysics3d::Transform(
            reactphysics3d::Vector3(pos.x, pos.y, pos.z),
            reactphysics3d::Quaternion(rot.x, rot.y, rot.z, rot.w)));
        rigidBody_->setLinearVelocity(reactphysics3d::Vector3(0.0f, 0.0f, 0.0f));
        rigidBody_->setAngularVelocity(reactphysics3d::Vector3(0.0f, 0.0f, 0.0f));
    }

protected:
    void Initialize() override {
        TryInitialize();
    }

    void Update() override {
        TryInitialize();
        if (!rigidBody_) return;
        auto *ctx = GetOwnerObjectContext();
        if (!ctx) return;
        auto *tr = ctx->GetComponent<Transform>();
        if (!tr) return;

        const auto transform = rigidBody_->getTransform();
        const auto pos = transform.getPosition();
        const auto rot = transform.getOrientation();

        tr->SetTranslate(Vector3{pos.x, pos.y, pos.z});
        tr->SetRotateQuaternion(Quaternion{rot.x, rot.y, rot.z, rot.w});
    }

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

        // 使用する形状（Colliderコンポーネント）の選択
        const auto colliders = GetOwnerColliders();
        auto *current = GetSelectedCollider();
        const std::string preview = current ? current->GetComponentType() : "(Any)";
        if (ImGui::BeginCombo("Collider Shape", preview.c_str())) {
            if (ImGui::Selectable("(Any)", !current)) {
                SetSelectedCollider(nullptr);
            }
            for (auto *collider : colliders) {
                if (collider->Is2D()) continue;
                ImGui::PushID(collider);
                const bool selected = (collider == current);
                if (ImGui::Selectable(collider->GetComponentType().c_str(), selected)) {
                    SetSelectedCollider(collider);
                }
                ImGui::PopID();
            }
            ImGui::EndCombo();
        }
    }
#endif

    JSON SaveToJson() const override {
        JSON json = JSON::object();
        json["selectedColliderTypeName"] = selectedColliderTypeName_;
        json["selectedColliderOccurrenceIndex"] = selectedColliderOccurrenceIndex_;
        json["bodyType"] = static_cast<int>(bodyType_);
        json["mass"] = mass_;
        json["useGravity"] = useGravity_;
        json["interpolate"] = interpolate_;
        return json;
    }

    bool LoadFromJson(const JSON &json) override {
        selectedColliderTypeName_ = json.value("selectedColliderTypeName", std::string{});
        selectedColliderOccurrenceIndex_ = json.value("selectedColliderOccurrenceIndex", 0);
        bodyType_ = static_cast<reactphysics3d::BodyType>(json.value("bodyType", static_cast<int>(reactphysics3d::BodyType::DYNAMIC)));
        mass_ = json.value("mass", 1.0f);
        useGravity_ = json.value("useGravity", true);
        interpolate_ = json.value("interpolate", true);
        return true;
    }

private:
    bool TryInitialize() {
        if (isInitialized_) return true;
        auto *sceneCtx = GetOwnerSceneContext();
        auto *colliderComp = sceneCtx ? sceneCtx->GetComponent<SceneObjectCollider>() : nullptr;
        if (!sceneCtx || !colliderComp) return false;
        world_ = colliderComp->GetCollider()->GetPhysicsWorld();
        if (!world_) return false;
        auto *ctx = GetOwnerObjectContext();
        if (!ctx) return false;
        auto *tr = ctx->GetComponent<Transform>();
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

    /// @brief 使用するコライダーのコンポーネント型名（空の場合は未選択＝どのコライダーでも使用可）
    std::string selectedColliderTypeName_;
    /// @brief 同一型のコライダーが複数ある場合の何番目かを示すインデックス
    int selectedColliderOccurrenceIndex_ = 0;
};

REGISTER_COMPONENT_OBJECT(RigidBody3D)

} // namespace KashipanEngine
