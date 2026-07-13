#pragma once

#include <bitset>
#include <cstddef>
#include <cstdint>
#include <functional>
#include <unordered_map>
#include <variant>
#include <vector>

#include "Math/Vector3.h"

#include "Objects/MathObjects/2D/Capsule2D.h"
#include "Objects/MathObjects/2D/Circle.h"
#include "Objects/MathObjects/2D/Point2D.h"
#include "Objects/MathObjects/2D/Rect.h"
#include "Objects/MathObjects/2D/Segment.h"

#include <reactphysics3d/reactphysics3d.h>

namespace KashipanEngine {

class EmptyObject;
class ICollider;

struct HitInfo final {
    bool isHit = false;
    Vector3 normal{0.0f, 0.0f, 0.0f};
    float penetration = 0.0f;
};

struct HitInfo2D final {
    bool isHit = false;
    Vector3 normal{0.0f, 0.0f, 0.0f};
    float penetration = 0.0f;

    EmptyObject* selfObject = nullptr;
    EmptyObject* otherObject = nullptr;
    /// @brief 衝突判定を行った自分側/相手側のコライダーコンポーネント
    ICollider* selfCollider = nullptr;
    ICollider* otherCollider = nullptr;
};

struct HitInfo3D final {
    bool isHit = false;
    Vector3 normal{0.0f, 0.0f, 0.0f};
    float penetration = 0.0f;

    EmptyObject* selfObject = nullptr;
    EmptyObject* otherObject = nullptr;
    /// @brief 衝突判定を行った自分側/相手側のコライダーコンポーネント
    ICollider* selfCollider = nullptr;
    ICollider* otherCollider = nullptr;
};

struct ColliderInfo2D final {
    static constexpr std::size_t kMaxAttributes = 32;

    using ShapeVariant = std::variant<
        Math::Point2D,
        Math::Circle,
        Math::Rect,
        Math::Segment2D,
        Math::Capsule2D>;

    ShapeVariant shape{};
    EmptyObject* ownerObject = nullptr;
    /// @brief この情報を生成したICollider（RigidBodyの使用コライダー選択に使用。SceneObjectColliderが設定する）
    ICollider* sourceCollider = nullptr;

    std::bitset<kMaxAttributes> attribute{};
    std::bitset<kMaxAttributes> ignoreAttribute{};

    std::function<void(const HitInfo2D &hitInfo)> onCollisionEnter;
    std::function<void(const HitInfo2D &hitInfo)> onCollisionStay;
    std::function<void(const HitInfo2D &hitInfo)> onCollisionExit;

    bool enabled = true;
    /// @brief 連続衝突判定（CCD）。高速移動時に移動経路を分割して判定し、すり抜けを検出する
    bool continuousDetection = false;
};

struct ColliderInfo3D final {
    static constexpr std::size_t kMaxAttributes = 32;

    struct SphereShape3D final {
        Vector3 center{0.0f, 0.0f, 0.0f};
        float radius = 0.0f;
    };

    struct BoxShape3D final {
        Vector3 center{0.0f, 0.0f, 0.0f};
        Vector3 halfExtents{0.0f, 0.0f, 0.0f};
    };

    struct CapsuleShape3D final {
        Vector3 center{0.0f, 0.0f, 0.0f};
        float radius = 0.0f;
        float height = 0.0f;
    };

    struct ConvexMeshShape3D final {
        std::vector<Vector3> vertices{};
        std::vector<std::uint32_t> indices{};
        Vector3 scale{1.0f, 1.0f, 1.0f};
    };

    struct ConcaveMeshShape3D final {
        std::vector<Vector3> vertices{};
        std::vector<std::uint32_t> indices{};
        Vector3 scale{ 1.0f, 1.0f, 1.0f };
    };

    struct HeightFieldShape3D final {
        std::vector<float> heights{};
        std::uint32_t width = 0;
        std::uint32_t length = 0;
        float minHeight = 0.0f;
        float maxHeight = 0.0f;
        Vector3 scale{1.0f, 1.0f, 1.0f};
    };

    using ShapeVariant = std::variant<
        SphereShape3D,
        BoxShape3D,
        CapsuleShape3D,
        ConvexMeshShape3D,
        ConcaveMeshShape3D,
        HeightFieldShape3D>;

    ShapeVariant shape{};
    EmptyObject* ownerObject = nullptr;
    /// @brief この情報を生成したICollider（RigidBodyの使用コライダー選択に使用。SceneObjectColliderが設定する）
    ICollider* sourceCollider = nullptr;

    std::bitset<kMaxAttributes> attribute{};
    std::bitset<kMaxAttributes> ignoreAttribute{};

    std::function<void(const HitInfo3D &hitInfo)> onCollisionEnter;
    std::function<void(const HitInfo3D &hitInfo)> onCollisionStay;
    std::function<void(const HitInfo3D &hitInfo)> onCollisionExit;

    bool enabled = true;
    /// @brief 連続衝突判定（CCD）。高速移動時に移動経路を分割して判定し、すり抜けを検出する
    bool continuousDetection = false;
};

class Collider final {
public:
    using ColliderID = std::uint32_t;
    using PhysicsWorld = reactphysics3d::PhysicsWorld;
    using RigidBody = reactphysics3d::RigidBody;
    using ColliderHandle = reactphysics3d::Collider;
    using CollisionCallback = reactphysics3d::CollisionCallback;

    struct HitPair2D {
        ColliderID a = 0;
        ColliderID b = 0;
    };

    struct HitPair3D {
        ColliderID a = 0;
        ColliderID b = 0;
    };

    Collider();
    ~Collider();

    ColliderID Add(const ColliderInfo2D &info);
    ColliderID Add(const ColliderInfo3D &info);

    bool Remove2D(ColliderID id);
    bool Remove3D(ColliderID id);

    bool UpdateColliderInfo2D(ColliderID id, const ColliderInfo2D &info);
    bool UpdateColliderInfo3D(ColliderID id, const ColliderInfo3D &info);

    void Clear2D();
    void Clear3D();

    std::vector<HitPair2D> CheckAll2D() const;
    std::vector<HitPair3D> CheckAll3D() const;

    bool Check2D(ColliderID a, ColliderID b) const;
    bool Check3D(ColliderID a, ColliderID b) const;

    void Update2D();
    void Update3D();

    void StepPhysics(float timeStep);

    PhysicsWorld *GetPhysicsWorld() { return physicsWorld_; }
    const PhysicsWorld *GetPhysicsWorld() const { return physicsWorld_; }


private:
    struct ShapeHandle3D {
        reactphysics3d::CollisionShape *shape = nullptr;
        reactphysics3d::ConvexMesh *convexMesh = nullptr;
        reactphysics3d::ConcaveMeshShape *concaveMesh = nullptr;
        reactphysics3d::HeightField *heightField = nullptr;
    };

    struct ColliderRuntime3D {
        RigidBody *body = nullptr;
        ColliderHandle *collider = nullptr;
        ShapeHandle3D shape;
        bool ownsBody = false;
    };

    struct CollisionEvent3D {
        ColliderID a = 0;
        ColliderID b = 0;
        HitInfo3D hitInfoA{};
        HitInfo3D hitInfoB{};
    };

    template<typename Info>
    struct Entry {
        ColliderID id;
        Info info;
        ColliderRuntime3D runtime{};
        /// @brief 連続衝突判定用の前フレーム位置（3Dはボディ位置、2Dは形状のバウンディング中心）
        /// @details CCDが有効かつ有効状態の間だけ毎フレーム記録される。ランタイムは毎フレーム
        ///          再構築されるため、フレームを跨ぐ情報はEntry側に保持する
        bool hasPrevPosition = false;
        Vector3 prevPosition{ 0.0f, 0.0f, 0.0f };
    };

    template<typename TEntry>
    static bool EraseById(std::vector<TEntry> &v, ColliderID id) {
        for (auto it = v.begin(); it != v.end(); ++it) {
            if (it->id == id) {
                v.erase(it);
                return true;
            }
        }
        return false;
    }

    const Entry<ColliderInfo2D> *Find2D(ColliderID id) const;
    const Entry<ColliderInfo3D> *Find3D(ColliderID id) const;
    Entry<ColliderInfo3D> *Find3D(ColliderID id);

    static std::uint64_t MakePairKey(ColliderID a, ColliderID b);

    void EnsureWorldCreated();
    void ReleaseWorld();

    bool BuildRuntime3D(Entry<ColliderInfo3D> &entry);
    void ReleaseRuntime3D(Entry<ColliderInfo3D> &entry);
    bool UpdateRuntime3D(Entry<ColliderInfo3D> &entry, const ColliderInfo3D &info);
    bool UpdateColliderShape3D(Entry<ColliderInfo3D> &entry, const ColliderInfo3D &info);
    bool UpdateColliderTransform3D(Entry<ColliderInfo3D> &entry, const ColliderInfo3D &info);
    std::optional<ShapeHandle3D> CreateShape3D(const ColliderInfo3D &info);
    reactphysics3d::Transform MakeTransform3D(const ColliderInfo3D &info) const;
    reactphysics3d::Vector3 ToRp3d(const Vector3 &v) const;
    Vector3 FromRp3d(const reactphysics3d::Vector3 &v) const;
    HitInfo3D BuildHitInfo3D(const reactphysics3d::CollisionCallback::ContactPoint &contact) const;

    void Dispatch2D(ColliderID a, ColliderID b, const HitInfo2D &hitInfo, bool wasHit);
    void Dispatch3D(ColliderID a, ColliderID b, const HitInfo3D &hitInfoA, const HitInfo3D &hitInfoB, bool wasHit);

    /// @brief 2Dの連続衝突判定（スイープ）。CCD有効コライダーの移動経路の中間位置で判定し、
    ///        検出したヒットをペアキー（ID昇順。HitInfoは小さいID側を自分として計算）で収集する
    void CollectContinuousHits2D(std::unordered_map<std::uint64_t, HitInfo2D> &outHits);
    /// @brief 2Dコライダーの現在位置を次フレームのスイープ用に記録する
    void RecordPrevPositions2D();
    /// @brief 3Dコライダーの現在位置を次フレームのスイープ用に記録する
    void RecordPrevPositions3D();

    std::vector<std::uint64_t> prevPairs2D_;
    std::vector<std::uint64_t> prevPairs3D_;
    std::vector<CollisionEvent3D> frameEvents3D_;
    std::vector<std::uint64_t> curPairs3D_;

    reactphysics3d::PhysicsCommon physicsCommon_{};
    reactphysics3d::PhysicsWorld *physicsWorld_ = nullptr;

    ColliderID nextId_ = 1;
    std::vector<Entry<ColliderInfo2D>> colliders2D_;
    std::vector<Entry<ColliderInfo3D>> colliders3D_;

    float accumulatedTime_ = 0.0f;
};

} // namespace KashipanEngine
