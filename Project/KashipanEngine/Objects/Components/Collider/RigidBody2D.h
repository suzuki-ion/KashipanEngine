#pragma once
#include <algorithm>
#include <vector>

#include "Objects/ObjectComponentHeader.h"
#include "Objects/Components/Collider/ICollider.h"
#include "Objects/Components/Transform.h"
#include "Scene/Components/SceneObjectCollider.h"
#include "Math/Vector2.h"
#include "Math/Vector3.h"
#include "Utilities/Translation.h"

#include <box2d/box2d.h>

namespace KashipanEngine {

/// @brief 2D用の物理挙動（速度・重力・衝突応答）を持たせるコンポーネント
/// @details RigidBody3DがReactPhysics3Dの`RigidBody`を薄くラップするのと同じ要領で、Box2Dの
///          `b2BodyId`を薄くラップする。ボディ自体はこのコンポーネントが生成・所有し（Initialize時に
///          ベアボディとして生成）、`Objects/Collision/Collider.cpp`（Collider::BuildRuntime2D）が
///          対象のColliderコンポーネントの形状をこのボディへシェイプとして取り付ける。
///          衝突応答（反発・摩擦・めり込み解消等）はBox2D本体が内部で解決するため、このクラスは
///          パラメータの受け渡しと、Transformとの同期（Update時に物理→Transformへ反映）のみを行う
class RigidBody2D final : public IObjectComponent {
public:
    OBJECT_COMPONENT_CONSTRUCTOR(RigidBody2D, 1,
        ADD_MEMBER_VARIABLE_WITH_CALLBACK(mass_, [this] { SetMass(mass_); });
        ADD_MEMBER_VARIABLE_WITH_CALLBACK(useGravity_, [this] { SetUseGravity(useGravity_); });
        ADD_MEMBER_VARIABLE_WITH_CALLBACK(restitution_, [this] { SetRestitution(restitution_); });
        ADD_MEMBER_VARIABLE_WITH_CALLBACK(friction_, [this] { SetFriction(friction_); });
    )
    COMPONENT_CATEGORY("Collision")
    ~RigidBody2D() override = default;

    std::unique_ptr<IObjectComponent> Clone() const override {
        auto ptr = std::make_unique<RigidBody2D>();
        ptr->mass_ = mass_;
        ptr->useGravity_ = useGravity_;
        ptr->restitution_ = restitution_;
        ptr->friction_ = friction_;
        ptr->selectedColliderTypeName_ = selectedColliderTypeName_;
        ptr->selectedColliderOccurrenceIndex_ = selectedColliderOccurrenceIndex_;
        return ptr;
    }

    //==================================================
    // 物理パラメータ
    //==================================================

    void SetVelocity(const Vector2 &velocity) {
        if (!B2_IS_NULL(body_)) b2Body_SetLinearVelocity(body_, b2Vec2{ velocity.x, velocity.y });
    }
    Vector2 GetVelocity() const {
        if (B2_IS_NULL(body_)) return Vector2{ 0.0f, 0.0f };
        const b2Vec2 v = b2Body_GetLinearVelocity(body_);
        return Vector2{ v.x, v.y };
    }
    /// @brief 角速度を設定する（ラジアン/秒、Z軸周り。正の値がCCW方向）
    void SetAngularVelocity(float angularVelocity) {
        if (!B2_IS_NULL(body_)) b2Body_SetAngularVelocity(body_, angularVelocity);
    }
    float GetAngularVelocity() const {
        return B2_IS_NULL(body_) ? 0.0f : b2Body_GetAngularVelocity(body_);
    }
    void SetMass(float mass) {
        mass_ = std::max(mass, kMinMass);
        ApplyMass();
    }
    float GetMass() const noexcept { return mass_; }
    void SetUseGravity(bool enabled) {
        useGravity_ = enabled;
        if (!B2_IS_NULL(body_)) b2Body_SetGravityScale(body_, enabled ? 1.0f : 0.0f);
    }
    bool IsGravityEnabled() const noexcept { return useGravity_; }
    /// @brief 反発係数（0=衝突後に反発しない、1=完全弾性衝突相当）
    void SetRestitution(float restitution) {
        restitution_ = std::clamp(restitution, 0.0f, 1.0f);
        ApplyMaterial();
    }
    float GetRestitution() const noexcept { return restitution_; }
    /// @brief 摩擦係数（クーロン摩擦、0=無摩擦）
    void SetFriction(float friction) {
        friction_ = std::max(friction, 0.0f);
        ApplyMaterial();
    }
    float GetFriction() const noexcept { return friction_; }

    //==================================================
    // Box2D連携
    //==================================================

    /// @brief このRigidBody2Dが所有するBox2Dのボディを取得する（未生成の場合はnull）
    /// @details Collider::BuildRuntime2Dが、対象コライダーの形状をこのボディへ取り付けるために使用する
    b2BodyId GetRigidBodyId() const noexcept { return body_; }

    /// @brief 現在のTransformの位置・回転を物理ボディへ反映する
    /// @details エディターでオブジェクトを移動させても、既に生成済みの物理ボディの位置は
    ///          自動的には追従しない（毎フレームUpdateで行っているのは物理→Transformへの反映のみ）。
    ///          そのままPlayを開始すると、物理ボディが生成された時点の古い位置（多くの場合原点）へ
    ///          Transformが引き戻されてしまうため、Play開始時にこれを呼んで同期を取る
    ///          （RigidBody3D::SyncFromTransformと同じ役割）
    void SyncFromTransform() {
        if (B2_IS_NULL(body_)) return;
        auto *ctx = GetOwnerObjectContext();
        auto *tr = ctx ? ctx->GetComponent<Transform>() : nullptr;
        if (!tr) return;

        const Vector3 pos = tr->GetTranslate();
        b2Body_SetTransform(body_, b2Vec2{ pos.x, pos.y }, b2MakeRot(tr->GetRotate().z));
        b2Body_SetLinearVelocity(body_, b2Vec2{ 0.0f, 0.0f });
        b2Body_SetAngularVelocity(body_, 0.0f);
    }

    /// @brief Collider::BuildRuntime2Dが、対象コライダーのBox2Dシェイプを（作り直しも含めて）
    ///        生成し終えた直後に呼ぶ
    /// @details Box2Dのシェイプは対象コライダー側の形状同期のたびに（つまり毎フレーム）破棄・再生成
    ///          されるため、その生成時に使うb2ShapeDefは既定値（密度1・反発係数0等）に戻ってしまう。
    ///          mass_/restitution_/friction_をここで毎回上書きし直すことで、シェイプが作り直されても
    ///          設定値が失われないようにする
    void ReapplyPhysicalProperties() {
        ApplyMass();
        ApplyMaterial();
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
            if (auto *collider = dynamic_cast<ICollider *>(pair.first)) {
                result.push_back(collider);
            }
        }
        return result;
    }

protected:
    void Initialize() override {
        TryInitialize();
    }

    void Finalize() override {
        // B2_IS_NULLは未設定判定のみで、既に破棄済み（無効化）されたIDかどうかは分からないため
        // b2Body_IsValidで生存確認してから破棄する（Collider::ReleaseRuntime2Dと同じ理由）
        if (!B2_IS_NULL(body_) && b2Body_IsValid(body_)) {
            b2DestroyBody(body_);
        }
        body_ = b2_nullBodyId;
    }

    void Update() override {
        TryInitialize();
        if (B2_IS_NULL(body_)) return;
        auto *ctx = GetOwnerObjectContext();
        auto *tr = ctx ? ctx->GetComponent<Transform>() : nullptr;
        if (!tr) return;

        const b2Vec2 pos = b2Body_GetPosition(body_);
        const b2Rot rot = b2Body_GetRotation(body_);
        const Vector3 translate = tr->GetTranslate();
        tr->SetTranslate(Vector3{ pos.x, pos.y, translate.z });

        Vector3 rotate = tr->GetRotate();
        rotate.z = b2Rot_GetAngle(rot);
        tr->SetRotate(rotate);
    }

#if defined(USE_IMGUI)
    void ShowImGui() override {
        if (ImGui::DragFloat(TranslationLabel("component.rigidbody2d.mass"), &mass_, 0.01f, kMinMass, 1000.0f)) {
            SetMass(mass_);
        }
        if (ImGui::Checkbox(TranslationLabel("component.rigidbody2d.usegravity"), &useGravity_)) {
            SetUseGravity(useGravity_);
        }
        if (ImGui::DragFloat(TranslationLabel("component.rigidbody2d.restitution"), &restitution_, 0.01f, 0.0f, 1.0f)) {
            SetRestitution(restitution_);
        }
        if (ImGui::DragFloat(TranslationLabel("component.rigidbody2d.friction"), &friction_, 0.01f, 0.0f, 10.0f)) {
            SetFriction(friction_);
        }

        // 使用する形状（Colliderコンポーネント）の選択
        const auto colliders = GetOwnerColliders();
        auto *current = GetSelectedCollider();
        const std::string preview = current ? current->GetComponentType() : "(Any)";
        if (ImGui::BeginCombo(TranslationLabel("component.rigidbody2d.collider_shape"), preview.c_str())) {
            if (ImGui::Selectable(TranslationLabel("component.rigidbody2d.any"), !current)) {
                SetSelectedCollider(nullptr);
            }
            for (auto *collider : colliders) {
                if (!collider->Is2D()) continue;
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
        JSON json{
            {"mass", mass_}, {"useGravity", useGravity_},
            {"restitution", restitution_}, {"friction", friction_},
        };
        json["selectedColliderTypeName"] = selectedColliderTypeName_;
        json["selectedColliderOccurrenceIndex"] = selectedColliderOccurrenceIndex_;
        return json;
    }
    bool LoadFromJson(const JSON &json) override {
        mass_ = std::max(json.value("mass", 1.0f), kMinMass);
        useGravity_ = json.value("useGravity", true);
        restitution_ = std::clamp(json.value("restitution", 0.3f), 0.0f, 1.0f);
        friction_ = std::max(json.value("friction", 0.3f), 0.0f);
        selectedColliderTypeName_ = json.value("selectedColliderTypeName", std::string{});
        selectedColliderOccurrenceIndex_ = json.value("selectedColliderOccurrenceIndex", 0);
        ApplyMass();
        ApplyMaterial();
        if (!B2_IS_NULL(body_)) b2Body_SetGravityScale(body_, useGravity_ ? 1.0f : 0.0f);
        return true;
    }

private:
    /// @brief 質量の下限（0除算・無限大の逆質量を避けるため）
    static constexpr float kMinMass = 0.001f;

    /// @brief 所属するSceneObjectColliderのBox2Dワールドへベアボディ（形状なし）を生成する
    /// @details 実際の形状の取り付けはCollider::BuildRuntime2D（対象コライダー側のシェイプ同期時）が行う
    bool TryInitialize() {
        if (!B2_IS_NULL(body_)) return true;
        auto *sceneCtx = GetOwnerSceneContext();
        auto *colliderComp = sceneCtx ? sceneCtx->GetComponent<SceneObjectCollider>() : nullptr;
        if (!sceneCtx || !colliderComp) return false;
        const b2WorldId world = colliderComp->GetCollider()->GetPhysicsWorld2D();
        if (B2_IS_NULL(world)) return false;
        auto *ctx = GetOwnerObjectContext();
        auto *tr = ctx ? ctx->GetComponent<Transform>() : nullptr;
        if (!tr) return false;

        b2BodyDef def = b2DefaultBodyDef();
        def.type = b2_dynamicBody;
        const Vector3 pos = tr->GetTranslate();
        def.position = b2Vec2{ pos.x, pos.y };
        def.rotation = b2MakeRot(tr->GetRotate().z);
        def.gravityScale = useGravity_ ? 1.0f : 0.0f;

        body_ = b2CreateBody(world, &def);
        return !B2_IS_NULL(body_);
    }

    /// @brief mass_をボディへ反映する（形状由来の慣性モーメント分布は保ったまま、質量だけ上書きする）
    void ApplyMass() {
        if (B2_IS_NULL(body_) || b2Body_GetShapeCount(body_) <= 0) return;
        b2Body_ApplyMassFromShapes(body_);
        b2MassData massData = b2Body_GetMassData(body_);
        if (massData.mass > 1e-6f) {
            massData.rotationalInertia *= mass_ / massData.mass;
        }
        massData.mass = mass_;
        b2Body_SetMassData(body_, massData);
    }

    /// @brief restitution_/friction_を、ボディに取り付いている全シェイプへ反映する
    void ApplyMaterial() {
        if (B2_IS_NULL(body_)) return;
        const int count = b2Body_GetShapeCount(body_);
        if (count <= 0) return;
        std::vector<b2ShapeId> shapes(static_cast<std::size_t>(count));
        const int filled = b2Body_GetShapes(body_, shapes.data(), count);
        for (int i = 0; i < filled; ++i) {
            b2Shape_SetRestitution(shapes[i], restitution_);
            b2Shape_SetFriction(shapes[i], friction_);
        }
    }

    b2BodyId body_ = b2_nullBodyId;
    float mass_ = 1.0f;
    bool useGravity_ = true;
    /// @brief 反発係数（0=反発しない、1=完全弾性衝突相当）
    float restitution_ = 0.3f;
    /// @brief 摩擦係数（クーロン摩擦）
    float friction_ = 0.3f;

    /// @brief 使用するコライダーのコンポーネント型名（空の場合は未選択＝どのコライダーでも使用可）
    std::string selectedColliderTypeName_;
    /// @brief 同一型のコライダーが複数ある場合の何番目かを示すインデックス
    int selectedColliderOccurrenceIndex_ = 0;
};

REGISTER_COMPONENT_OBJECT(RigidBody2D)

} // namespace KashipanEngine
