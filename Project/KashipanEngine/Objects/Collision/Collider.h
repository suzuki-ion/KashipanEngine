#pragma once

#include <bitset>
#include <cstddef>
#include <cstdint>
#include <functional>
#include <unordered_map>
#include <variant>
#include <vector>

#include "Debug/Logger.h"
#include "Math/Vector3.h"

#include "Objects/MathObjects/2D/Capsule2D.h"
#include "Objects/MathObjects/2D/Circle.h"
#include "Objects/MathObjects/2D/Point2D.h"
#include "Objects/MathObjects/2D/Rect.h"
#include "Objects/MathObjects/2D/Segment.h"

#include <box2d/box2d.h>
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
    /// @brief トリガー。物理的な押し戻し（衝突応答）を行わず、衝突コールバックの通知のみ行う
    bool isTrigger = false;
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
    /// @brief トリガー。物理的な押し戻し（衝突応答）を行わず、衝突コールバックの通知のみ行う
    bool isTrigger = false;
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

    std::vector<HitPair3D> CheckAll3D() const;

    bool Check3D(ColliderID a, ColliderID b) const;

    /// @brief ReactPhysics3DのCollider*から、それを登録した際の情報（ownerObject/sourceCollider）を取得する
    /// @details RayCollider等、自身は常駐形状を持たないコライダーがレイキャストのヒット結果を
    ///          衝突相手側のEmptyObject/ICollider（の情報）へ逆引きするために使用する
    /// @return 見つからない場合はnullptr
    const ColliderInfo3D *FindInfoByHandle3D(const ColliderHandle *handle) const;

    void Update2D();
    void Update3D();

    void StepPhysics(float timeStep);

    PhysicsWorld *GetPhysicsWorld() { return physicsWorld_; }
    const PhysicsWorld *GetPhysicsWorld() const { return physicsWorld_; }
    /// @brief 2D用のBox2Dワールドを取得する（RigidBody2Dがベアボディを生成する際に使用）
    b2WorldId GetPhysicsWorld2D() const { return world2D_; }


private:
    struct ShapeHandle3D {
        reactphysics3d::CollisionShape *shape = nullptr;
        reactphysics3d::ConvexMesh *convexMesh = nullptr;
        reactphysics3d::ConcaveMeshShape *concaveMesh = nullptr;
        reactphysics3d::TriangleMesh *triangleMesh = nullptr;
        reactphysics3d::HeightField *heightField = nullptr;
    };

    struct ColliderRuntime3D {
        RigidBody *body = nullptr;
        ColliderHandle *collider = nullptr;
        ShapeHandle3D shape;
        bool ownsBody = false;
    };

    /// @brief 2D用のランタイム状態（Box2Dのボディ・シェイプへのハンドル）
    /// @details ColliderRuntime3Dと対称。bodyはRigidBody2Dが選択されている場合はそちらが所有する
    ///          ボディを共有（ownsBody=false）、無ければColliderが所有する静的ボディを生成する（ownsBody=true）。
    ///          Box2Dの接触の継続性（反発・摩擦の計算に影響する）を保つため、形状が変化していない限り
    ///          ボディ・シェイプは毎フレーム作り直さず使い回す（lastOrigin/lastAngleは、ownsBody=trueの
    ///          静的ボディについて、シェイプを壊さずTransformだけ同期すべきかを判定するための前回値）
    struct ColliderRuntime2D {
        b2BodyId body = b2_nullBodyId;
        b2ShapeId shape = b2_nullShapeId;
        bool ownsBody = false;
        Vector2 lastOrigin{ 0.0f, 0.0f };
        float lastAngle = 0.0f;
        bool hasLastOrigin = false;
    };

    struct CollisionEvent3D {
        ColliderID a = 0;
        ColliderID b = 0;
        HitInfo3D hitInfoA{};
        HitInfo3D hitInfoB{};
    };

    template<typename Info, typename Runtime>
    struct Entry {
        ColliderID id;
        Info info;
        Runtime runtime{};
        /// @brief 連続衝突判定用の前フレーム位置（3Dはボディ位置、2Dは形状のバウンディング中心）
        /// @details CCDが有効かつ有効状態の間だけ毎フレーム記録される。ランタイムは毎フレーム
        ///          再構築されるため、フレームを跨ぐ情報はEntry側に保持する
        bool hasPrevPosition = false;
        Vector3 prevPosition{ 0.0f, 0.0f, 0.0f };
    };

    template<typename TEntry>
    static bool EraseById(std::vector<TEntry> &v, ColliderID id) {
        LogScope scope;
        for (auto it = v.begin(); it != v.end(); ++it) {
            if (it->id == id) {
                v.erase(it);
                return true;
            }
        }
        return false;
    }

    const Entry<ColliderInfo2D, ColliderRuntime2D> *Find2D(ColliderID id) const;
    const Entry<ColliderInfo3D, ColliderRuntime3D> *Find3D(ColliderID id) const;
    Entry<ColliderInfo3D, ColliderRuntime3D> *Find3D(ColliderID id);

    static std::uint64_t MakePairKey(ColliderID a, ColliderID b);

    void EnsureWorldCreated();
    void ReleaseWorld();

    bool BuildRuntime3D(Entry<ColliderInfo3D, ColliderRuntime3D> &entry);
    void ReleaseRuntime3D(Entry<ColliderInfo3D, ColliderRuntime3D> &entry);
    bool UpdateRuntime3D(Entry<ColliderInfo3D, ColliderRuntime3D> &entry, const ColliderInfo3D &info);
    bool UpdateColliderShape3D(Entry<ColliderInfo3D, ColliderRuntime3D> &entry, const ColliderInfo3D &info);
    bool UpdateColliderTransform3D(Entry<ColliderInfo3D, ColliderRuntime3D> &entry, const ColliderInfo3D &info);
    std::optional<ShapeHandle3D> CreateShape3D(const ColliderInfo3D &info);
    reactphysics3d::Transform MakeTransform3D(const ColliderInfo3D &info) const;
    reactphysics3d::Vector3 ToRp3d(const Vector3 &v) const;
    Vector3 FromRp3d(const reactphysics3d::Vector3 &v) const;
    HitInfo3D BuildHitInfo3D(const reactphysics3d::CollisionCallback::ContactPoint &contact) const;

    /// @brief RigidBody2Dの選択状況に応じて、対象のColliderInfo2Dが使うべきBox2DボディID
    ///        （RigidBody2Dが所有するベアボディ、または無ければColliderが所有する静的ボディ）を用意する
    bool BuildRuntime2D(Entry<ColliderInfo2D, ColliderRuntime2D> &entry);
    void ReleaseRuntime2D(Entry<ColliderInfo2D, ColliderRuntime2D> &entry);
    /// @brief ColliderInfo2D::ShapeVariantからBox2Dシェイプを（既存シェイプを破棄してから）生成し直す
    bool RecreateShape2D(Entry<ColliderInfo2D, ColliderRuntime2D> &entry);
    b2BodyDef MakeBodyDef2D(const ColliderInfo2D &info) const;
    /// @brief 形状（サイズ・トリガー設定等）は変化していない静的ボディ（ownsBody=true）について、
    ///        Transformの現在位置・回転だけをボディへ同期する（シェイプは壊さない）。
    ///        RigidBody2D所有の動的ボディには使わない（物理側が位置の実質的な所有者のため）
    void SyncStaticBodyTransform2D(Entry<ColliderInfo2D, ColliderRuntime2D> &entry);

    /// @brief 通常（非センサー）の接触ペアの現在の法線・めり込み量を、idA側の接触データから取得する
    /// @return 見つからない（接触していない等）場合はfalse
    bool QueryContactHitInfo2D(ColliderID idA, ColliderID idB, HitInfo2D &outHitInfo) const;
    /// @brief センサーコライダー（isTrigger）がvisitorと現在も重なっているかを取得する
    bool QuerySensorHitInfo2D(ColliderID sensorId, ColliderID visitorId) const;

    void Dispatch2D(ColliderID a, ColliderID b, const HitInfo2D &hitInfo, bool wasHit);
    void Dispatch3D(ColliderID a, ColliderID b, const HitInfo3D &hitInfoA, const HitInfo3D &hitInfoB, bool wasHit);

    /// @brief 3Dコライダーの現在位置を次フレームのスイープ用に記録する
    void RecordPrevPositions3D();

    std::vector<std::uint64_t> prevPairs2D_;
    std::vector<std::uint64_t> prevPairs3D_;
    std::vector<CollisionEvent3D> frameEvents3D_;
    std::vector<std::uint64_t> curPairs3D_;

    reactphysics3d::PhysicsCommon physicsCommon_{};
    reactphysics3d::PhysicsWorld *physicsWorld_ = nullptr;
    b2WorldId world2D_ = b2_nullWorldId;

    ColliderID nextId_ = 1;
    std::vector<Entry<ColliderInfo2D, ColliderRuntime2D>> colliders2D_;
    std::vector<Entry<ColliderInfo3D, ColliderRuntime3D>> colliders3D_;

    float accumulatedTime3D_ = 0.0f;
    float accumulatedTime2D_ = 0.0f;
};

} // namespace KashipanEngine
