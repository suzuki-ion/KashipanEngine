#include "Collider.h"

#include "Objects/EmptyObject.h"
#include "Objects/Components/Transform.h"
#include "Objects/Components/Collider/RigidBody2D.h"
#include "Objects/Components/Collider/RigidBody3D.h"
#include "Utilities/TimeUtils.h"
#include <algorithm>
#include <cmath>
#include <limits>
#include <optional>
#include <unordered_map>
#include <unordered_set>

namespace KashipanEngine {

namespace {
struct Bounds3D {
    Vector3 min{0.0f, 0.0f, 0.0f};
    Vector3 max{0.0f, 0.0f, 0.0f};
};

struct IndexPair {
    std::size_t a = 0;
    std::size_t b = 0;

    bool operator==(const IndexPair &o) const noexcept {
        return a == o.a && b == o.b;
    }
};

struct IndexPairHash {
    std::size_t operator()(const IndexPair &p) const noexcept {
        std::size_t h1 = std::hash<std::size_t>{}(p.a);
        std::size_t h2 = std::hash<std::size_t>{}(p.b);
        return h1 ^ (h2 + 0x9e3779b9u + (h1 << 6) + (h1 >> 2));
    }
};

struct GridKey3D {
    int x = 0;
    int y = 0;
    int z = 0;

    bool operator==(const GridKey3D &o) const noexcept {
        return x == o.x && y == o.y && z == o.z;
    }
};

struct GridKey3DHash {
    std::size_t operator()(const GridKey3D &k) const noexcept {
        std::size_t h = std::hash<int>{}(k.x);
        h ^= std::hash<int>{}(k.y) + 0x9e3779b9u + (h << 6) + (h >> 2);
        h ^= std::hash<int>{}(k.z) + 0x9e3779b9u + (h << 6) + (h >> 2);
        return h;
    }
};

inline IndexPair MakeIndexPair(std::size_t a, std::size_t b) {
    if (a > b) std::swap(a, b);
    return IndexPair{a, b};
}

inline int ToCell(float v, float cellSize) {
    return static_cast<int>(std::floor(v / cellSize));
}

std::optional<Bounds3D> ComputeBounds3D(const ColliderInfo3D::ShapeVariant &shape) {
    return std::visit(
        [](const auto &s) -> std::optional<Bounds3D> {
            using S = std::decay_t<decltype(s)>;

            if constexpr (std::is_same_v<S, ColliderInfo3D::SphereShape3D>) {
                const Vector3 r{s.radius, s.radius, s.radius};
                return Bounds3D{s.center - r, s.center + r};
            } else if constexpr (std::is_same_v<S, ColliderInfo3D::BoxShape3D>) {
                return Bounds3D{s.center - s.halfExtents, s.center + s.halfExtents};
            } else if constexpr (std::is_same_v<S, ColliderInfo3D::CapsuleShape3D>) {
                const float halfHeight = 0.5f * s.height;
                const Vector3 ext{s.radius, s.radius + halfHeight, s.radius};
                return Bounds3D{s.center - ext, s.center + ext};
            } else if constexpr (std::is_same_v<S, ColliderInfo3D::ConvexMeshShape3D>) {
                if (s.vertices.empty()) return std::nullopt;
                Vector3 minV = s.vertices.front();
                Vector3 maxV = s.vertices.front();
                for (const auto &v : s.vertices) {
                    minV.x = std::min(minV.x, v.x);
                    minV.y = std::min(minV.y, v.y);
                    minV.z = std::min(minV.z, v.z);
                    maxV.x = std::max(maxV.x, v.x);
                    maxV.y = std::max(maxV.y, v.y);
                    maxV.z = std::max(maxV.z, v.z);
                }
                minV = Vector3{ minV.x * s.scale.x, minV.y * s.scale.y, minV.z * s.scale.z };
                maxV = Vector3{ maxV.x * s.scale.x, maxV.y * s.scale.y, maxV.z * s.scale.z };
                return Bounds3D{ minV, maxV };
            } else if constexpr (std::is_same_v<S, ColliderInfo3D::ConcaveMeshShape3D>) {
                if (s.vertices.empty()) return std::nullopt;
                Vector3 minV = s.vertices.front();
                Vector3 maxV = s.vertices.front();
                for (const auto &v : s.vertices) {
                    minV.x = std::min(minV.x, v.x);
                    minV.y = std::min(minV.y, v.y);
                    minV.z = std::min(minV.z, v.z);
                    maxV.x = std::max(maxV.x, v.x);
                    maxV.y = std::max(maxV.y, v.y);
                    maxV.z = std::max(maxV.z, v.z);
                }
                minV = Vector3{ minV.x * s.scale.x, minV.y * s.scale.y, minV.z * s.scale.z };
                maxV = Vector3{ maxV.x * s.scale.x, maxV.y * s.scale.y, maxV.z * s.scale.z };
                return Bounds3D{ minV, maxV };
            } else if constexpr (std::is_same_v<S, ColliderInfo3D::HeightFieldShape3D>) {
                if (s.width == 0 || s.length == 0 || s.heights.empty()) return std::nullopt;
                const Vector3 ext{
                    0.5f * static_cast<float>(s.width - 1) * s.scale.x,
                    0.5f * (s.maxHeight - s.minHeight) * s.scale.y,
                    0.5f * static_cast<float>(s.length - 1) * s.scale.z};
                return Bounds3D{Vector3{-ext.x, s.minHeight * s.scale.y, -ext.z}, Vector3{ext.x, s.maxHeight * s.scale.y, ext.z}};
            } else {
                return std::nullopt;
            }
        },
        shape);
}

template<typename TColliders>
std::vector<IndexPair> BuildCandidatePairs3D(const TColliders &colliders) {
    constexpr float kCellSize = 16.0f;
    constexpr int kMaxCellsPerShape = 512;

    std::unordered_map<GridKey3D, std::vector<std::size_t>, GridKey3DHash> grid;
    std::vector<std::size_t> active;
    std::vector<std::size_t> global;

    active.reserve(colliders.size());
    global.reserve(colliders.size());

    for (std::size_t i = 0; i < colliders.size(); ++i) {
        const auto &c = colliders[i];
        if (!c.info.enabled) continue;
        active.push_back(i);

        const auto bounds = ComputeBounds3D(c.info.shape);
        if (!bounds.has_value()) {
            global.push_back(i);
            continue;
        }

        const int minX = ToCell(bounds->min.x, kCellSize);
        const int minY = ToCell(bounds->min.y, kCellSize);
        const int minZ = ToCell(bounds->min.z, kCellSize);
        const int maxX = ToCell(bounds->max.x, kCellSize);
        const int maxY = ToCell(bounds->max.y, kCellSize);
        const int maxZ = ToCell(bounds->max.z, kCellSize);

        const int cellsX = (maxX - minX + 1);
        const int cellsY = (maxY - minY + 1);
        const int cellsZ = (maxZ - minZ + 1);
        const int cellCount = cellsX * cellsY * cellsZ;
        if (cellsX <= 0 || cellsY <= 0 || cellsZ <= 0 || cellsX > kMaxCellsPerShape || cellsY > kMaxCellsPerShape || cellsZ > kMaxCellsPerShape || cellCount > kMaxCellsPerShape) {
            global.push_back(i);
            continue;
        }

        for (int z = minZ; z <= maxZ; ++z) {
            for (int y = minY; y <= maxY; ++y) {
                for (int x = minX; x <= maxX; ++x) {
                    auto &cell = grid[GridKey3D{x, y, z}];
                    if (cell.empty()) {
                        cell.reserve(8);
                    }
                    cell.push_back(i);
                }
            }
        }
    }

    std::size_t estimatedPairs = 0;
    for (const auto &[_, indices] : grid) {
        const std::size_t n = indices.size();
        if (n > 1) {
            estimatedPairs += (n * (n - 1)) / 2;
        }
    }
    estimatedPairs += global.size() * active.size();

    std::unordered_set<IndexPair, IndexPairHash> uniquePairs;
    uniquePairs.reserve(estimatedPairs);

    for (const auto &[_, indices] : grid) {
        for (std::size_t i = 0; i < indices.size(); ++i) {
            for (std::size_t j = i + 1; j < indices.size(); ++j) {
                uniquePairs.insert(MakeIndexPair(indices[i], indices[j]));
            }
        }
    }

    for (std::size_t gi = 0; gi < global.size(); ++gi) {
        const std::size_t g = global[gi];
        for (std::size_t a : active) {
            if (a == g) continue;
            uniquePairs.insert(MakeIndexPair(g, a));
        }
    }

    std::vector<IndexPair> out;
    out.reserve(uniquePairs.size());
    for (const auto &p : uniquePairs) out.emplace_back(p);
    return out;
}

inline bool ShouldTest(
    const std::bitset<ColliderInfo2D::kMaxAttributes> &selfAttr,
    const std::bitset<ColliderInfo2D::kMaxAttributes> &selfIgnore,
    const std::bitset<ColliderInfo2D::kMaxAttributes> &otherAttr) {
    (void)selfAttr;
    return (selfIgnore & otherAttr).none();
}

inline bool Intersects3D(const ColliderInfo3D::ShapeVariant &a, const ColliderInfo3D::ShapeVariant &b) {
    (void)a;
    (void)b;
    return false;
}

inline HitInfo3D ComputeHit3D(const ColliderInfo3D::ShapeVariant &a, const ColliderInfo3D::ShapeVariant &b) {
    (void)a;
    (void)b;
    return HitInfo3D{};
}

//==================================================
// 連続衝突判定（CCD）用ヘルパー
//==================================================

/// @brief スイープの1ステップあたりの許容移動量の下限（縮退形状・点などの保険）
constexpr float kMinSweepStep = 0.05f;
/// @brief スイープの最大分割数（テレポート等の巨大な移動でも計算量が爆発しないよう制限する）
constexpr int kMaxSweepSubsteps = 16;

/// @brief 3D形状の最小半径を求める
float ComputeMinHalfExtent3D(const ColliderInfo3D::ShapeVariant &shape) {
    const auto bounds = ComputeBounds3D(shape);
    if (!bounds) return kMinSweepStep;
    const Vector3 size = bounds->max - bounds->min;
    return std::max(kMinSweepStep, std::min({ size.x, size.y, size.z }) * 0.5f);
}

//==================================================
// Box2D連携用ヘルパー
//==================================================

inline b2Vec2 ToB2(const Vector2 &v) { return b2Vec2{v.x, v.y}; }
inline Vector2 FromB2(const b2Vec2 &v) { return Vector2{v.x, v.y}; }

/// @brief ワールド空間の2D形状を、指定の原点・角度を基準としたローカル空間へ変換する
/// @details ColliderInfo2D::shapeはICollider::BuildColliderInfo2D()により常にワールド空間で
///          構築されているが、Box2Dのシェイプはボディのローカル空間で定義する必要があるため、
///          ボディに使うのと同じ原点・角度（ICollider::GetSyncedOwnerPosition/
///          GetSyncedOwnerRotationEuler().z）でここへ変換してから渡す
ColliderInfo2D::ShapeVariant ToLocalShape2D(const ColliderInfo2D::ShapeVariant &shape, const Vector2 &origin, float angle) {
    const float c = std::cos(-angle);
    const float s = std::sin(-angle);
    const auto toLocal = [&](const Vector2 &worldPoint) {
        const Vector2 d = worldPoint - origin;
        return Vector2(d.x * c - d.y * s, d.x * s + d.y * c);
    };

    ColliderInfo2D::ShapeVariant result = shape;
    std::visit(
        [&](auto &s) {
            using S = std::decay_t<decltype(s)>;
            if constexpr (std::is_same_v<S, Math::Point2D>) {
                s.position = toLocal(s.position);
            } else if constexpr (std::is_same_v<S, Math::Circle>) {
                s.center = toLocal(s.center);
            } else if constexpr (std::is_same_v<S, Math::Rect>) {
                s.center = toLocal(s.center);
                s.rotation -= angle;
            } else if constexpr (std::is_same_v<S, Math::Segment2D>) {
                s.start = toLocal(s.start);
                s.end = toLocal(s.end);
            } else if constexpr (std::is_same_v<S, Math::Capsule2D>) {
                s.start = toLocal(s.start);
                s.end = toLocal(s.end);
            }
        },
        result);
    return result;
}

/// @brief 2Dコライダー情報から、対応するRigidBody2Dを取得する（RigidBody3Dと同じ規約）
/// @details RigidBody2Dが使用コライダーを明示的に選択している場合は、そのコライダー由来の
///          ColliderInfo2Dに対してのみ関連付ける（未選択の場合はどのコライダーでも関連付ける）
RigidBody2D *ResolveRigidBody2D(const ColliderInfo2D &info) {
    if (!info.ownerObject) return nullptr;
    auto *rb = info.ownerObject->GetComponent<RigidBody2D>();
    if (!rb) return nullptr;
    auto *selected = rb->GetSelectedCollider();
    if (selected && selected != info.sourceCollider) return nullptr;
    return rb;
}

/// @brief 2D形状同士が（Box2Dのシェイプ再生成が必要という意味で）等しいかを比較する
/// @brief 2D形状同士が、ワールド位置・回転の違いを除いた「形状パラメータ」として等しいかを比較する
/// @details ColliderInfo2D::shapeはICollider::BuildColliderInfo2D()により毎フレームワールド空間で
///          再構築されるため、中心座標やRectのrotationには現在のTransformの位置・回転がそのまま
///          含まれている。動的（RigidBody2D所有）な物体は毎フレーム位置・回転が変わるのが通常のため、
///          それらをそのまま比較すると常に「形状が変わった」と誤判定してしまい、Box2Dのシェイプが
///          毎フレーム作り直され、接触の継続性が失われて反発・摩擦が正しく働かなくなる。
///          そのため位置・回転は無視し、サイズ等の純粋な形状パラメータだけを比較する
///          （位置・回転の同期はBuildRuntime2D/SyncStaticBodyTransform2D側が別途担う）
bool ShapeVariantEquals2D(const ColliderInfo2D::ShapeVariant &a, const ColliderInfo2D::ShapeVariant &b) {
    if (a.index() != b.index()) return false;
    return std::visit(
        [&](const auto &sa) -> bool {
            using S = std::decay_t<decltype(sa)>;
            const auto &sb = std::get<S>(b);
            if constexpr (std::is_same_v<S, Math::Point2D>) {
                return true; // 位置以外にパラメータが無い
            } else if constexpr (std::is_same_v<S, Math::Circle>) {
                return sa.radius == sb.radius;
            } else if constexpr (std::is_same_v<S, Math::Rect>) {
                return sa.halfSize.x == sb.halfSize.x && sa.halfSize.y == sb.halfSize.y;
            } else if constexpr (std::is_same_v<S, Math::Segment2D>) {
                const float lenA2 = (sa.end - sa.start).LengthSquared();
                const float lenB2 = (sb.end - sb.start).LengthSquared();
                return lenA2 == lenB2;
            } else { // Capsule2D
                const float lenA2 = (sa.end - sa.start).LengthSquared();
                const float lenB2 = (sb.end - sb.start).LengthSquared();
                return lenA2 == lenB2 && sa.radius == sb.radius;
            }
        },
        a);
}

/// @brief 2つのColliderInfo2Dの間で、Box2Dのボディ・シェイプを作り直す必要があるかを判定する
/// @details Transformの位置・回転だけの変化はここでは対象外（形状自体は変わらないため、
///          静的ボディならSyncStaticBodyTransform2Dで、動的ボディなら物理側でそれぞれ扱う）。
///          Box2Dの接触の継続性（反発・摩擦の計算精度に影響する）を保つため、本当に形状・トリガー・
///          属性フィルタが変わった時だけ作り直すようにしている
bool RuntimeNeedsRebuild2D(const ColliderInfo2D &oldInfo, const ColliderInfo2D &newInfo) {
    if (oldInfo.enabled != newInfo.enabled) return true;
    if (oldInfo.isTrigger != newInfo.isTrigger) return true;
    if (oldInfo.attribute != newInfo.attribute) return true;
    if (oldInfo.ignoreAttribute != newInfo.ignoreAttribute) return true;
    return !ShapeVariantEquals2D(oldInfo.shape, newInfo.shape);
}

} // namespace

Collider::Collider() {
    EnsureWorldCreated();
}

Collider::~Collider() {
    ReleaseWorld();
}

Collider::ColliderID Collider::Add(const ColliderInfo2D &info) {
    const ColliderID id = nextId_++;
    colliders2D_.push_back({id, info});
    auto &entry = colliders2D_.back();
    BuildRuntime2D(entry);
    return id;
}

Collider::ColliderID Collider::Add(const ColliderInfo3D &info) {
    const ColliderID id = nextId_++;
    colliders3D_.push_back({id, info});
    auto &entry = colliders3D_.back();
    BuildRuntime3D(entry);
    return id;
}

bool Collider::Remove2D(ColliderID id) {
    for (auto it = colliders2D_.begin(); it != colliders2D_.end(); ++it) {
        if (it->id == id) {
            ReleaseRuntime2D(*it);
            colliders2D_.erase(it);
            return true;
        }
    }
    return false;
}

bool Collider::Remove3D(ColliderID id) {
    for (auto it = colliders3D_.begin(); it != colliders3D_.end(); ++it) {
        if (it->id == id) {
            ReleaseRuntime3D(*it);
            colliders3D_.erase(it);
            return true;
        }
    }
    return false;
}

bool Collider::UpdateColliderInfo2D(ColliderID id, const ColliderInfo2D &info) {
    for (auto &e : colliders2D_) {
        if (e.id != id) continue;

        const bool needsRebuild = B2_IS_NULL(e.runtime.body) || B2_IS_NULL(e.runtime.shape)
            || RuntimeNeedsRebuild2D(e.info, info);
        e.info = info;

        if (needsRebuild) {
            BuildRuntime2D(e);
        } else if (e.runtime.ownsBody) {
            // 形状自体は変わっていない。静的ボディ（RigidBody2D無し）はTransform側だけが動く
            // （移動床等）可能性があるため、接触の継続性を壊すシェイプの作り直しはせず位置だけ同期する
            SyncStaticBodyTransform2D(e);
        }
        return true;
    }
    return false;
}

bool Collider::UpdateColliderInfo3D(ColliderID id, const ColliderInfo3D &info) {
    for (auto &e : colliders3D_) {
        if (e.id == id) {
            UpdateRuntime3D(e, info);
            e.info = info;
            return true;
        }
    }
    return false;
}

void Collider::Clear2D() {
    for (auto &entry : colliders2D_) {
        ReleaseRuntime2D(entry);
    }
    colliders2D_.clear();
    prevPairs2D_.clear();
}

void Collider::Clear3D() {
    for (auto &entry : colliders3D_) {
        ReleaseRuntime3D(entry);
    }
    colliders3D_.clear();
    prevPairs3D_.clear();
    frameEvents3D_.clear();
    curPairs3D_.clear();
}

std::vector<Collider::HitPair3D> Collider::CheckAll3D() const {
    std::vector<HitPair3D> hits;
    if (!physicsWorld_) return hits;

    std::unordered_map<const ColliderHandle *, ColliderID> colliderIdByHandle;
    std::unordered_map<ColliderID, const ColliderInfo3D *> infoById;
    colliderIdByHandle.reserve(colliders3D_.size());
    infoById.reserve(colliders3D_.size());

    for (const auto &entry : colliders3D_) {
        if (!entry.runtime.collider) continue;
        colliderIdByHandle.emplace(entry.runtime.collider, entry.id);
        infoById.emplace(entry.id, &entry.info);
    }

    std::unordered_set<std::uint64_t> uniquePairs;

    struct Collector final : reactphysics3d::CollisionCallback {
        const std::unordered_map<const ColliderHandle *, ColliderID> &idMap;
        const std::unordered_map<ColliderID, const ColliderInfo3D *> &infoMap;
        std::unordered_set<std::uint64_t> &pairs;

        Collector(
            const std::unordered_map<const ColliderHandle *, ColliderID> &idMap,
            const std::unordered_map<ColliderID, const ColliderInfo3D *> &infoMap,
            std::unordered_set<std::uint64_t> &pairs)
            : idMap(idMap), infoMap(infoMap), pairs(pairs) {}

        void onContact(const CallbackData &callbackData) override {
            const int pairCount = callbackData.getNbContactPairs();
            for (int i = 0; i < pairCount; ++i) {
                const auto &pair = callbackData.getContactPair(i);
                const auto *colliderA = pair.getCollider1();
                const auto *colliderB = pair.getCollider2();
                if (!colliderA || !colliderB) continue;

                auto itA = idMap.find(colliderA);
                auto itB = idMap.find(colliderB);
                if (itA == idMap.end() || itB == idMap.end()) continue;

                const ColliderID idA = itA->second;
                const ColliderID idB = itB->second;

                auto infoAIt = infoMap.find(idA);
                auto infoBIt = infoMap.find(idB);
                if (infoAIt == infoMap.end() || infoBIt == infoMap.end()) continue;

                const auto &infoA = *infoAIt->second;
                const auto &infoB = *infoBIt->second;
                if (!infoA.enabled || !infoB.enabled) continue;
                if (!ShouldTest(infoA.attribute, infoA.ignoreAttribute, infoB.attribute) ||
                    !ShouldTest(infoB.attribute, infoB.ignoreAttribute, infoA.attribute)) {
                    continue;
                }

                pairs.insert(MakePairKey(idA, idB));
            }
        }
    };

    Collector collector(colliderIdByHandle, infoById, uniquePairs);
    physicsWorld_->testCollision(collector);

    hits.reserve(uniquePairs.size());
    for (auto key : uniquePairs) {
        const ColliderID a = static_cast<ColliderID>(key >> 32);
        const ColliderID b = static_cast<ColliderID>(key & 0xffffffffu);
        hits.push_back({a, b});
    }
    return hits;
}

bool Collider::Check3D(ColliderID a, ColliderID b) const {
    const auto *pa = Find3D(a);
    const auto *pb = Find3D(b);
    if (!pa || !pb) return false;
    if (!pa->info.enabled || !pb->info.enabled) return false;

    if (!ShouldTest(pa->info.attribute, pa->info.ignoreAttribute, pb->info.attribute) ||
        !ShouldTest(pb->info.attribute, pb->info.ignoreAttribute, pa->info.attribute)) {
        return false;
    }

    if (!physicsWorld_ || !pa->runtime.collider || !pb->runtime.collider) return false;

    struct Collector final : reactphysics3d::CollisionCallback {
        bool hit = false;
        const ColliderHandle *targetA = nullptr;
        const ColliderHandle *targetB = nullptr;

        Collector(const ColliderHandle *a, const ColliderHandle *b) : targetA(a), targetB(b) {}

        void onContact(const CallbackData &callbackData) override {
            const int pairCount = callbackData.getNbContactPairs();
            for (int i = 0; i < pairCount; ++i) {
                const auto &pair = callbackData.getContactPair(i);
                const auto *colliderA = pair.getCollider1();
                const auto *colliderB = pair.getCollider2();
                if ((colliderA == targetA && colliderB == targetB) || (colliderA == targetB && colliderB == targetA)) {
                    hit = true;
                    return;
                }
            }
        }
    };

    Collector collector(pa->runtime.collider, pb->runtime.collider);
    physicsWorld_->testCollision(collector);
    return collector.hit;
}

const ColliderInfo3D *Collider::FindInfoByHandle3D(const ColliderHandle *handle) const {
    if (!handle) return nullptr;
    for (const auto &entry : colliders3D_) {
        if (entry.runtime.collider == handle) return &entry.info;
    }
    return nullptr;
}

std::uint64_t Collider::MakePairKey(ColliderID a, ColliderID b) {
    if (a > b) std::swap(a, b);
    return (static_cast<std::uint64_t>(a) << 32) | static_cast<std::uint64_t>(b);
}

void Collider::Dispatch2D(ColliderID a, ColliderID b, const HitInfo2D &hitInfo, bool wasHit) {
    const auto *ea = Find2D(a);
    const auto *eb = Find2D(b);
    if (!ea || !eb) return;

    HitInfo2D hiA = hitInfo;
    hiA.selfObject = ea->info.ownerObject;
    hiA.otherObject = eb->info.ownerObject;
    hiA.selfCollider = ea->info.sourceCollider;
    hiA.otherCollider = eb->info.sourceCollider;

    HitInfo2D hiB = hitInfo;
    hiB.selfObject = eb->info.ownerObject;
    hiB.otherObject = ea->info.ownerObject;
    hiB.selfCollider = eb->info.sourceCollider;
    hiB.otherCollider = ea->info.sourceCollider;
    // hitInfo.normal は ComputeHit(A, B) により「BからAへ向かう方向
    // （Aの押し出し方向）」で計算されるため、B側が受け取る法線
    // （AからBへ向かう＝Bの押し出し方向）はその逆向きになる。
    hiB.normal = -hitInfo.normal;

    const bool isHitNow = hitInfo.isHit;

    if (isHitNow) {
        if (!wasHit) {
            if (ea->info.onCollisionEnter) ea->info.onCollisionEnter(hiA);
            if (eb->info.onCollisionEnter) eb->info.onCollisionEnter(hiB);
        }
        if (ea->info.onCollisionStay) ea->info.onCollisionStay(hiA);
        if (eb->info.onCollisionStay) eb->info.onCollisionStay(hiB);
    } else {
        if (wasHit) {
            if (ea->info.onCollisionExit) ea->info.onCollisionExit(hiA);
            if (eb->info.onCollisionExit) eb->info.onCollisionExit(hiB);
        }
    }
}

void Collider::Dispatch3D(ColliderID a, ColliderID b, const HitInfo3D &hitInfoA, const HitInfo3D &hitInfoB, bool wasHit) {
    const auto *ea = Find3D(a);
    const auto *eb = Find3D(b);
    if (!ea || !eb) return;

    HitInfo3D hiA = hitInfoA;
    hiA.selfObject = ea->info.ownerObject;
    hiA.otherObject = eb->info.ownerObject;
    hiA.selfCollider = ea->info.sourceCollider;
    hiA.otherCollider = eb->info.sourceCollider;
    hiA.normal = hitInfoA.normal;

    HitInfo3D hiB = hitInfoB;
    hiB.selfObject = eb->info.ownerObject;
    hiB.otherObject = ea->info.ownerObject;
    hiB.selfCollider = eb->info.sourceCollider;
    hiB.otherCollider = ea->info.sourceCollider;
    hiB.normal = hitInfoB.normal;

    const bool isHitNow = hitInfoA.isHit && hitInfoB.isHit;

    if (isHitNow) {
        if (!wasHit) {
            if (ea->info.onCollisionEnter) ea->info.onCollisionEnter(hiA);
            if (eb->info.onCollisionEnter) eb->info.onCollisionEnter(hiB);
        }
        if (ea->info.onCollisionStay) ea->info.onCollisionStay(hiA);
        if (eb->info.onCollisionStay) eb->info.onCollisionStay(hiB);
    } else {
        if (wasHit) {
            if (ea->info.onCollisionExit) ea->info.onCollisionExit(hiA);
            if (eb->info.onCollisionExit) eb->info.onCollisionExit(hiB);
        }
    }
}

bool Collider::QueryContactHitInfo2D(ColliderID idA, ColliderID idB, HitInfo2D &outHitInfo) const {
    const auto *ea = Find2D(idA);
    const auto *eb = Find2D(idB);
    if (!ea || !eb || B2_IS_NULL(ea->runtime.shape) || B2_IS_NULL(eb->runtime.shape)) return false;

    // 1形状あたりの同時接触数（多角形の頂点数上限＋αの余裕を見た値）は実運用上十分な大きさ
    constexpr int kMaxContacts = 16;
    b2ContactData contacts[kMaxContacts];
    const int count = b2Shape_GetContactData(ea->runtime.shape, contacts, kMaxContacts);
    for (int i = 0; i < count; ++i) {
        const b2ContactData &cd = contacts[i];
        const bool aIsShapeA = B2_ID_EQUALS(cd.shapeIdA, ea->runtime.shape);
        const b2ShapeId otherShape = aIsShapeA ? cd.shapeIdB : cd.shapeIdA;
        if (!B2_ID_EQUALS(otherShape, eb->runtime.shape)) continue;
        if (cd.manifold.pointCount <= 0) continue;

        float minSeparation = cd.manifold.points[0].separation;
        for (int p = 1; p < cd.manifold.pointCount; ++p) {
            minSeparation = std::min(minSeparation, cd.manifold.points[p].separation);
        }

        // b2Manifold::normalは「クエリしたshapeId（=aIsShapeAならA、そうでなければB）→相手」の向き。
        // Dispatch2DはidA→idBの向きを期待するため、順序が逆なら反転する
        Vector2 normal = FromB2(cd.manifold.normal);
        if (!aIsShapeA) normal = -normal;

        outHitInfo = HitInfo2D{};
        outHitInfo.isHit = true;
        outHitInfo.normal = Vector3(normal.x, normal.y, 0.0f);
        outHitInfo.penetration = std::max(0.0f, -minSeparation);
        return true;
    }
    return false;
}

bool Collider::QuerySensorHitInfo2D(ColliderID sensorId, ColliderID visitorId) const {
    const auto *sensorEntry = Find2D(sensorId);
    if (!sensorEntry || B2_IS_NULL(sensorEntry->runtime.shape)) return false;
    const auto *visitorEntry = Find2D(visitorId);
    if (!visitorEntry || B2_IS_NULL(visitorEntry->runtime.shape)) return false;

    const int capacity = b2Shape_GetSensorCapacity(sensorEntry->runtime.shape);
    if (capacity <= 0) return false;
    std::vector<b2ShapeId> visitors(static_cast<std::size_t>(capacity));
    const int count = b2Shape_GetSensorData(sensorEntry->runtime.shape, visitors.data(), capacity);
    for (int i = 0; i < count; ++i) {
        if (B2_ID_EQUALS(visitors[i], visitorEntry->runtime.shape)) return true;
    }
    return false;
}

void Collider::Update2D() {
    if (B2_IS_NULL(world2D_)) return;

    // Box2Dは固定タイムステップを前提としたソルバーのため、Update3Dと同じ蓄積方式で
    // 可変フレームレートを吸収する（4サブステップはBox2D公式が推奨する既定値）
    constexpr float kDefaultTimeStep = 1.0f / 60.0f;
    constexpr int kSubStepCount = 4;
    const float deltaTime = GetDeltaTime();
    accumulatedTime2D_ += deltaTime;
    while (accumulatedTime2D_ >= kDefaultTimeStep) {
        b2World_Step(world2D_, kDefaultTimeStep, kSubStepCount);
        accumulatedTime2D_ -= kDefaultTimeStep;
    }

    // b2ShapeId(packed) -> ColliderID の逆引き
    std::unordered_map<std::uint64_t, ColliderID> idByShape;
    idByShape.reserve(colliders2D_.size());
    for (const auto &entry : colliders2D_) {
        if (B2_IS_NULL(entry.runtime.shape)) continue;
        idByShape.emplace(b2StoreShapeId(entry.runtime.shape), entry.id);
    }
    const auto lookupId = [&](b2ShapeId shapeId) -> ColliderID {
        const auto it = idByShape.find(b2StoreShapeId(shapeId));
        return it != idByShape.end() ? it->second : 0;
    };

    std::vector<std::uint64_t> cur;
    std::vector<std::uint64_t> handledThisFrame;

    // 通常（非センサー）の接触イベント。isSensorなシェイプ同士の組み合わせはここには出てこない
    const b2ContactEvents contactEvents = b2World_GetContactEvents(world2D_);
    for (int i = 0; i < contactEvents.beginCount; ++i) {
        const auto &ev = contactEvents.beginEvents[i];
        const ColliderID idA = lookupId(ev.shapeIdA);
        const ColliderID idB = lookupId(ev.shapeIdB);
        if (idA == 0 || idB == 0) continue;

        HitInfo2D hi;
        if (!QueryContactHitInfo2D(idA, idB, hi)) continue;

        const std::uint64_t key = MakePairKey(idA, idB);
        const bool wasHit = std::binary_search(prevPairs2D_.begin(), prevPairs2D_.end(), key);
        Dispatch2D(idA, idB, hi, wasHit);
        cur.push_back(key);
        handledThisFrame.push_back(key);
    }
    for (int i = 0; i < contactEvents.endCount; ++i) {
        const auto &ev = contactEvents.endEvents[i];
        const ColliderID idA = lookupId(ev.shapeIdA);
        const ColliderID idB = lookupId(ev.shapeIdB);
        if (idA == 0 || idB == 0) continue;

        Dispatch2D(idA, idB, HitInfo2D{}, true);
        handledThisFrame.push_back(MakePairKey(idA, idB));
    }

    // センサー（isTrigger）の重なりイベント。Box2Dでは接触イベントと別系統で、法線・めり込み量を
    // 持たない（物理的な接触ではなく重なり検知のため）。HitInfo2D側もisHitのみ立てて他は既定値のまま渡す
    const b2SensorEvents sensorEvents = b2World_GetSensorEvents(world2D_);
    for (int i = 0; i < sensorEvents.beginCount; ++i) {
        const auto &ev = sensorEvents.beginEvents[i];
        const ColliderID idA = lookupId(ev.sensorShapeId);
        const ColliderID idB = lookupId(ev.visitorShapeId);
        if (idA == 0 || idB == 0) continue;

        HitInfo2D hi{};
        hi.isHit = true;
        const std::uint64_t key = MakePairKey(idA, idB);
        const bool wasHit = std::binary_search(prevPairs2D_.begin(), prevPairs2D_.end(), key);
        Dispatch2D(idA, idB, hi, wasHit);
        cur.push_back(key);
        handledThisFrame.push_back(key);
    }
    for (int i = 0; i < sensorEvents.endCount; ++i) {
        const auto &ev = sensorEvents.endEvents[i];
        const ColliderID idA = lookupId(ev.sensorShapeId);
        const ColliderID idB = lookupId(ev.visitorShapeId);
        if (idA == 0 || idB == 0) continue;

        Dispatch2D(idA, idB, HitInfo2D{}, true);
        handledThisFrame.push_back(MakePairKey(idA, idB));
    }

    std::sort(handledThisFrame.begin(), handledThisFrame.end());

    // Begin/Endどちらのイベントも発生しなかった、前フレームから継続中のペアはStayを発火する
    for (const auto key : prevPairs2D_) {
        if (std::binary_search(handledThisFrame.begin(), handledThisFrame.end(), key)) continue;

        const ColliderID idA = static_cast<ColliderID>(key >> 32);
        const ColliderID idB = static_cast<ColliderID>(key & 0xffffffffu);

        HitInfo2D hi;
        if (QueryContactHitInfo2D(idA, idB, hi)) {
            Dispatch2D(idA, idB, hi, true);
            cur.push_back(key);
        } else if (QuerySensorHitInfo2D(idA, idB) || QuerySensorHitInfo2D(idB, idA)) {
            HitInfo2D sensorHit{};
            sensorHit.isHit = true;
            Dispatch2D(idA, idB, sensorHit, true);
            cur.push_back(key);
        }
        // どちらの問い合わせも失敗した場合は、既に破棄されたコライダー等で接触情報を復元できない
        // ケース。次フレーム以降にBox2D側のEndイベントが来ればそちらでExitが発火する
    }

    std::sort(cur.begin(), cur.end());
    cur.erase(std::unique(cur.begin(), cur.end()), cur.end());
    prevPairs2D_ = std::move(cur);
}

void Collider::RecordPrevPositions3D() {
    for (auto &entry : colliders3D_) {
        if (entry.info.continuousDetection && entry.info.enabled && entry.runtime.body) {
            entry.prevPosition = FromRp3d(entry.runtime.body->getTransform().getPosition());
            entry.hasPrevPosition = true;
        } else {
            entry.hasPrevPosition = false;
        }
    }
}

void Collider::Update3D() {
    if (!physicsWorld_) return;

    constexpr float kDefaultTimeStep = 1.0f / 60.0f;
    const float deltaTime = GetDeltaTime();
    accumulatedTime3D_ += deltaTime;

    while (accumulatedTime3D_ >= kDefaultTimeStep) {
        StepPhysics(kDefaultTimeStep);
        accumulatedTime3D_ -= kDefaultTimeStep;
    }

    frameEvents3D_.clear();
    curPairs3D_.clear();

    std::unordered_map<const ColliderHandle *, ColliderID> colliderIdByHandle;
    std::unordered_map<ColliderID, const ColliderInfo3D *> infoById;
    colliderIdByHandle.reserve(colliders3D_.size());
    infoById.reserve(colliders3D_.size());

    for (const auto &entry : colliders3D_) {
        if (!entry.runtime.collider) continue;
        colliderIdByHandle.emplace(entry.runtime.collider, entry.id);
        infoById.emplace(entry.id, &entry.info);
    }

    struct Collector final : reactphysics3d::CollisionCallback {
        const std::unordered_map<const ColliderHandle *, ColliderID> &idMap;
        const std::unordered_map<ColliderID, const ColliderInfo3D *> &infoMap;
        std::vector<CollisionEvent3D> &events;
        std::vector<std::uint64_t> &pairs;

        Collector(
            const std::unordered_map<const ColliderHandle *, ColliderID> &idMap,
            const std::unordered_map<ColliderID, const ColliderInfo3D *> &infoMap,
            std::vector<CollisionEvent3D> &events,
            std::vector<std::uint64_t> &pairs)
            : idMap(idMap), infoMap(infoMap), events(events), pairs(pairs) {}

        static HitInfo3D MakeHitInfo(const reactphysics3d::CollisionCallback::ContactPoint &contact) {
            HitInfo3D info;
            info.isHit = true;
            const auto normal = contact.getWorldNormal();
            info.normal = Vector3{normal.x, normal.y, normal.z};
            info.penetration = contact.getPenetrationDepth();
            return info;
        }

        void onContact(const CallbackData &callbackData) override {
            const int pairCount = callbackData.getNbContactPairs();
            for (int i = 0; i < pairCount; ++i) {
                const auto &pair = callbackData.getContactPair(i);
                const auto *colliderA = pair.getCollider1();
                const auto *colliderB = pair.getCollider2();
                if (!colliderA || !colliderB) continue;

                auto itA = idMap.find(colliderA);
                auto itB = idMap.find(colliderB);
                if (itA == idMap.end() || itB == idMap.end()) continue;

                const ColliderID idA = itA->second;
                const ColliderID idB = itB->second;

                auto infoAIt = infoMap.find(idA);
                auto infoBIt = infoMap.find(idB);
                if (infoAIt == infoMap.end() || infoBIt == infoMap.end()) continue;

                const auto &infoA = *infoAIt->second;
                const auto &infoB = *infoBIt->second;
                if (!infoA.enabled || !infoB.enabled) continue;
                if (!ShouldTest(infoA.attribute, infoA.ignoreAttribute, infoB.attribute) ||
                    !ShouldTest(infoB.attribute, infoB.ignoreAttribute, infoA.attribute)) {
                    continue;
                }

                HitInfo3D hitInfoA{};
                HitInfo3D hitInfoB{};
                if (pair.getNbContactPoints() > 0) {
                    // getWorldNormal() は collider1(A) から collider2(B) へ向かうベクトル。
                    // エンジンの規約（2D側の ComputeHit と同じ）では、各コライダーが
                    // 受け取る法線は「相手から自分へ向かう方向（押し出し方向）」なので、
                    // B側はそのまま、A側は逆向きになる。
                    hitInfoB = MakeHitInfo(pair.getContactPoint(0));
                    hitInfoA = hitInfoB;
                    hitInfoA.normal = -hitInfoB.normal;
                } else {
                    hitInfoA.isHit = true;
                    hitInfoB.isHit = true;
                }

                // Dispatch側は MakePairKey（ID昇順）で a/b を復元するため、
                // ここでも小さいID側を a に揃えておく。これがずれると、RP3Dの
                // collider1/collider2 の内部順序次第で HitInfo が入れ替わり、
                // 逆向きの法線が届いてしまう。
                ColliderID eventA = idA;
                ColliderID eventB = idB;
                if (eventA > eventB) {
                    std::swap(eventA, eventB);
                    std::swap(hitInfoA, hitInfoB);
                }

                events.push_back({eventA, eventB, hitInfoA, hitInfoB});
                pairs.push_back(MakePairKey(eventA, eventB));
            }
        }
    };

    Collector collector(colliderIdByHandle, infoById, frameEvents3D_, curPairs3D_);
    physicsWorld_->testCollision(collector);

    // 連続衝突判定（CCD）: 1フレームで形状サイズを超えて移動したコライダーは、
    // 移動経路の中間位置（終端は上の通常判定で検出済み）でも判定してすり抜けを検出する。
    // イベントは同じCollectorへ追加され、重複ペアは後段のsort/uniqueとhitMapの
    // emplace（先勝ち）によって通常判定・より早い時刻のヒットが優先される。
    // なお、テレポート（リスポーン等）でも経路上の判定が走るため、瞬間移動させる場合は
    // 移動前にCCDを無効にするか、コライダーを一度無効化すること。
    for (auto &entry : colliders3D_) {
        if (!entry.info.continuousDetection || !entry.info.enabled) continue;
        if (!entry.runtime.body || !entry.runtime.collider) continue;
        if (!entry.hasPrevPosition) continue;

        const reactphysics3d::Transform currentTransform = entry.runtime.body->getTransform();
        const Vector3 currentPosition = FromRp3d(currentTransform.getPosition());
        const Vector3 delta = currentPosition - entry.prevPosition;
        const float distance = delta.Length();

        // 1フレームの移動量が形状の最小半径以下なら、すり抜けは起きない（通常判定で検出できる）
        const float maxStep = ComputeMinHalfExtent3D(entry.info.shape);
        if (distance <= maxStep) continue;

        const int substeps = std::min(kMaxSweepSubsteps, static_cast<int>(std::ceil(distance / maxStep)));
        for (int i = 1; i < substeps; ++i) {
            const float t = static_cast<float>(i) / static_cast<float>(substeps);
            const Vector3 sweptPosition = entry.prevPosition + delta * t;
            entry.runtime.body->setTransform(
                reactphysics3d::Transform(ToRp3d(sweptPosition), currentTransform.getOrientation()));
            physicsWorld_->testCollision(entry.runtime.body, collector);
        }
        entry.runtime.body->setTransform(currentTransform);
    }

    std::sort(curPairs3D_.begin(), curPairs3D_.end());
    curPairs3D_.erase(std::unique(curPairs3D_.begin(), curPairs3D_.end()), curPairs3D_.end());

    std::unordered_map<std::uint64_t, std::pair<HitInfo3D, HitInfo3D>> hitMap;
    hitMap.reserve(frameEvents3D_.size());
    for (const auto &event : frameEvents3D_) {
        const std::uint64_t key = MakePairKey(event.a, event.b);
        hitMap.emplace(key, std::make_pair(event.hitInfoA, event.hitInfoB));
    }

    for (const auto &[key, hitInfo] : hitMap) {
        const ColliderID a = static_cast<ColliderID>(key >> 32);
        const ColliderID b = static_cast<ColliderID>(key & 0xffffffffu);
        const bool wasHit = std::binary_search(prevPairs3D_.begin(), prevPairs3D_.end(), key);
        Dispatch3D(a, b, hitInfo.first, hitInfo.second, wasHit);
    }

    for (const auto key : prevPairs3D_) {
        if (std::binary_search(curPairs3D_.begin(), curPairs3D_.end(), key)) continue;
        const ColliderID a = static_cast<ColliderID>(key >> 32);
        const ColliderID b = static_cast<ColliderID>(key & 0xffffffffu);
        Dispatch3D(a, b, HitInfo3D{}, HitInfo3D{}, true);
    }

    prevPairs3D_ = curPairs3D_;

    RecordPrevPositions3D();
}

const Collider::Entry<ColliderInfo2D, Collider::ColliderRuntime2D> *Collider::Find2D(ColliderID id) const {
    for (const auto &e : colliders2D_) {
        if (e.id == id) return &e;
    }
    return nullptr;
}

const Collider::Entry<ColliderInfo3D, Collider::ColliderRuntime3D> *Collider::Find3D(ColliderID id) const {
    for (const auto &e : colliders3D_) {
        if (e.id == id) return &e;
    }
    return nullptr;
}

Collider::Entry<ColliderInfo3D, Collider::ColliderRuntime3D> *Collider::Find3D(ColliderID id) {
    for (auto &e : colliders3D_) {
        if (e.id == id) return &e;
    }
    return nullptr;
}

void Collider::StepPhysics(float timeStep) {
    if (physicsWorld_) {
        physicsWorld_->update(timeStep);
    }
}

void Collider::EnsureWorldCreated() {
    if (!physicsWorld_) {
        reactphysics3d::PhysicsWorld::WorldSettings settings;
        settings.gravity = reactphysics3d::Vector3(0.0f, -9.81f, 0.0f);
        physicsWorld_ = physicsCommon_.createPhysicsWorld(settings);
    }
    if (B2_IS_NULL(world2D_)) {
        b2WorldDef def = b2DefaultWorldDef();
        // 3D側の物理ワールドと同じ大きさの重力に揃える
        def.gravity = b2Vec2{0.0f, -9.81f};
        // Box2Dの既定値（1.0 m/s）だと、その速度未満で衝突した場合に反発係数を設定していても
        // 一切反発しない（ジッター防止のための仕様）。小さい落下距離での動作確認等、低速の衝突でも
        // 反発係数の効果が見えるよう、ほぼ常に反発が有効になる程度まで下げておく
        def.restitutionThreshold = 0.01f;
        world2D_ = b2CreateWorld(&def);
    }
}

void Collider::ReleaseWorld() {
    if (physicsWorld_) {
        physicsCommon_.destroyPhysicsWorld(physicsWorld_);
        physicsWorld_ = nullptr;
    }
    if (!B2_IS_NULL(world2D_)) {
        b2DestroyWorld(world2D_);
        world2D_ = b2_nullWorldId;
    }
}

b2BodyDef Collider::MakeBodyDef2D(const ColliderInfo2D &info) const {
    b2BodyDef def = b2DefaultBodyDef();
    // RigidBody2Dが無い（＝Colliderが自前でボディを持つ）場合の既定は静的。
    // RigidBody2Dが見つかった場合はBuildRuntime2D側でDynamicへ差し替える
    def.type = b2_staticBody;
    def.isEnabled = info.enabled;
    if (info.sourceCollider) {
        def.position = ToB2(Vector2(info.sourceCollider->GetSyncedOwnerPosition()));
        def.rotation = b2MakeRot(info.sourceCollider->GetSyncedOwnerRotationEuler().z);
    }
    return def;
}

bool Collider::RecreateShape2D(Entry<ColliderInfo2D, ColliderRuntime2D> &entry) {
    // ReleaseRuntime2D同様、外部（RigidBody2D）が所有するボディの破棄により
    // シェイプが既に内部的に破棄済みの場合があるため、b2Shape_IsValidで生存確認してから破棄する
    if (!B2_IS_NULL(entry.runtime.shape) && b2Shape_IsValid(entry.runtime.shape)) {
        b2DestroyShape(entry.runtime.shape, true);
    }
    entry.runtime.shape = b2_nullShapeId;
    if (B2_IS_NULL(entry.runtime.body) || !b2Body_IsValid(entry.runtime.body)) return false;

    // シェイプ生成時、Box2Dのセグメント形状は静的ボディにしか付けられない。
    // Ray2DColliderが動的なRigidBody2Dと同じオブジェクトに付いている場合は生成をスキップする
    // （RigidBody3Dの凹メッシュ×動的ボディの組み合わせを弾いている既存の前例と同じ扱い）
    const bool isSegment = std::holds_alternative<Math::Segment2D>(entry.info.shape);
    if (isSegment && b2Body_GetType(entry.runtime.body) != b2_staticBody) {
        return false;
    }

    Vector2 origin{0.0f, 0.0f};
    float angle = 0.0f;
    if (b2Body_GetType(entry.runtime.body) == b2_staticBody) {
        // 静的ボディ（Colliderがこのフレームで新規生成した使い捨てボディ）はTransformの現在値を直接使う
        if (entry.info.sourceCollider) {
            origin = Vector2(entry.info.sourceCollider->GetSyncedOwnerPosition());
            angle = entry.info.sourceCollider->GetSyncedOwnerRotationEuler().z;
        }
    } else {
        // 動的ボディ（RigidBody2D所有）は、物理側が実際に把握している現在のボディ座標を基準にする。
        // Transformは1フレーム遅れて追従する値のため使わない（ズレる上、b2Body_SetTransformでの
        // 強制テレポートは接触の途切れ扱い・性能低下を招くため、動的ボディの位置には一切触れない）
        origin = FromB2(b2Body_GetPosition(entry.runtime.body));
        angle = b2Rot_GetAngle(b2Body_GetRotation(entry.runtime.body));
    }
    const auto localShape = ToLocalShape2D(entry.info.shape, origin, angle);

    b2ShapeDef def = b2DefaultShapeDef();
    def.isSensor = entry.info.isTrigger;
    def.enableContactEvents = !entry.info.isTrigger;
    def.enableSensorEvents = entry.info.isTrigger;
    def.density = 1.0f;
    def.filter.categoryBits = entry.info.attribute.any() ? entry.info.attribute.to_ullong() : B2_DEFAULT_CATEGORY_BITS;
    const std::uint64_t ignoreBits = entry.info.ignoreAttribute.to_ullong();
    // ignoreAttributeは「相手のattributeがこのビットと重なっていたら判定しない」という除外指定のため、
    // Box2DのmaskBits（「このビットと重なっていたら判定する」という許可指定）へは反転して渡す
    def.filter.maskBits = (ignoreBits == 0) ? B2_DEFAULT_MASK_BITS : ~ignoreBits;

    entry.runtime.shape = std::visit(
        [&](const auto &s) -> b2ShapeId {
            using S = std::decay_t<decltype(s)>;
            if constexpr (std::is_same_v<S, Math::Circle>) {
                const b2Circle circle{ToB2(s.center), s.radius};
                return b2CreateCircleShape(entry.runtime.body, &def, &circle);
            } else if constexpr (std::is_same_v<S, Math::Rect>) {
                const b2Polygon box = b2MakeOffsetBox(s.halfSize.x, s.halfSize.y, ToB2(s.center), b2MakeRot(s.rotation));
                return b2CreatePolygonShape(entry.runtime.body, &def, &box);
            } else if constexpr (std::is_same_v<S, Math::Capsule2D>) {
                const b2Capsule capsule{ToB2(s.start), ToB2(s.end), s.radius};
                return b2CreateCapsuleShape(entry.runtime.body, &def, &capsule);
            } else if constexpr (std::is_same_v<S, Math::Segment2D>) {
                const b2Segment segment{ToB2(s.start), ToB2(s.end)};
                return b2CreateSegmentShape(entry.runtime.body, &def, &segment);
            } else {
                // Point2D: 現状このシステムでは常駐形状として生成されない想定
                return b2_nullShapeId;
            }
        },
        localShape);

    return !B2_IS_NULL(entry.runtime.shape);
}

bool Collider::BuildRuntime2D(Entry<ColliderInfo2D, ColliderRuntime2D> &entry) {
    EnsureWorldCreated();
    if (B2_IS_NULL(world2D_)) return false;

    ReleaseRuntime2D(entry);

    // RigidBody2Dが使用コライダーを明示的に選択している場合は、そのコライダーだけを
    // RigidBodyへ取り付ける（未選択の場合は従来通りどのコライダーでも取り付ける）
    RigidBody2D *rb = ResolveRigidBody2D(entry.info);

    if (rb) {
        // 動的ボディは物理側が位置の実質的な所有者のため、ここではTransformの値で上書き
        // （テレポート）しない。初期位置はRigidBody2D::TryInitialize側でボディ生成時に一度だけ設定する
        entry.runtime.body = rb->GetRigidBodyId();
        entry.runtime.ownsBody = false;
    } else {
        const b2BodyDef def = MakeBodyDef2D(entry.info);
        entry.runtime.body = b2CreateBody(world2D_, &def);
        entry.runtime.ownsBody = true;
        entry.runtime.lastOrigin = FromB2(def.position);
        entry.runtime.lastAngle = b2Rot_GetAngle(def.rotation);
        entry.runtime.hasLastOrigin = true;
    }

    if (B2_IS_NULL(entry.runtime.body)) return false;
    if (!RecreateShape2D(entry)) return false;

    // ここに来るのは形状が新規作成・変更された時（呼び出し元がRuntimeNeedsRebuild2D等で判定済み）。
    // RigidBody2Dが指定している質量・反発係数・摩擦係数を、Box2D既定値（密度1・反発係数0等）で
    // 作られた新しいシェイプへ改めて適用する
    if (rb) rb->ReapplyPhysicalProperties();
    return true;
}

void Collider::SyncStaticBodyTransform2D(Entry<ColliderInfo2D, ColliderRuntime2D> &entry) {
    if (B2_IS_NULL(entry.runtime.body) || !entry.runtime.ownsBody) return;
    if (!entry.info.sourceCollider) return;

    const Vector2 origin = Vector2(entry.info.sourceCollider->GetSyncedOwnerPosition());
    const float angle = entry.info.sourceCollider->GetSyncedOwnerRotationEuler().z;
    if (entry.runtime.hasLastOrigin
        && origin.x == entry.runtime.lastOrigin.x && origin.y == entry.runtime.lastOrigin.y
        && angle == entry.runtime.lastAngle) {
        return; // 前回から変化なし。b2Body_SetTransformの呼び出し自体を避ける（接触の継続性を保つため）
    }

    b2Body_SetTransform(entry.runtime.body, ToB2(origin), b2MakeRot(angle));
    entry.runtime.lastOrigin = origin;
    entry.runtime.lastAngle = angle;
    entry.runtime.hasLastOrigin = true;
}

void Collider::ReleaseRuntime2D(Entry<ColliderInfo2D, ColliderRuntime2D> &entry) {
    // B2_IS_NULLはIDが「未設定（nullセンチネル）」かどうかしか見ておらず、シェイプが
    // 所有元ボディの破棄（RigidBody2D::Finalize等、Collider外の経路）によって既に内部的に
    // 破棄済みかどうかは判定できない。破棄済みIDに対してb2DestroyShapeを呼ぶと二重解放になり
    // クラッシュするため、必ずb2Shape_IsValid/b2Body_IsValidで生存確認してから破棄する
    if (!B2_IS_NULL(entry.runtime.shape) && b2Shape_IsValid(entry.runtime.shape)) {
        b2DestroyShape(entry.runtime.shape, true);
    }
    if (!B2_IS_NULL(entry.runtime.body) && entry.runtime.ownsBody && b2Body_IsValid(entry.runtime.body)) {
        b2DestroyBody(entry.runtime.body);
    }
    entry.runtime = {};
}

bool Collider::BuildRuntime3D(Entry<ColliderInfo3D, ColliderRuntime3D> &entry) {
    EnsureWorldCreated();
    if (!physicsWorld_) return false;

    ReleaseRuntime3D(entry);

    auto shapeHandle = CreateShape3D(entry.info);
    if (!shapeHandle.has_value()) return false;

    entry.runtime.shape = shapeHandle.value();
    const auto transform = MakeTransform3D(entry.info);
    const bool isConcaveMesh = std::holds_alternative<ColliderInfo3D::ConcaveMeshShape3D>(entry.info.shape);
    // RigidBody3Dが使用コライダーを明示的に選択している場合は、そのコライダーだけを
    // RigidBodyへ取り付ける（未選択の場合は従来通りどのコライダーでも取り付ける）
    auto *rb = entry.info.ownerObject->GetComponent<RigidBody3D>();
    if (rb) {
        auto *selected = rb->GetSelectedCollider();
        if (selected && selected != entry.info.sourceCollider) rb = nullptr;
    }
    // ReactPhysics3Dの非凸三角形メッシュは静的ボディ専用。Dynamic/KinematicなRigidBodyへ
    // 取り付けると物理シミュレーションの対象として扱えないため、ランタイムColliderを生成しない。
    if (isConcaveMesh && rb && rb->GetBodyType() != reactphysics3d::BodyType::STATIC) {
        ReleaseRuntime3D(entry);
        return false;
    }
    if (rb) {
        entry.runtime.body = rb->GetRigidBody();
        entry.runtime.ownsBody = false;
        entry.runtime.body->setTransform(transform);
    } else {
        entry.runtime.body = physicsWorld_->createRigidBody(transform);
        if (!entry.runtime.body) return false;
        entry.runtime.body->setType(reactphysics3d::BodyType::STATIC);
        entry.runtime.body->setIsActive(entry.info.enabled);
        entry.runtime.ownsBody = true;
    }

    entry.runtime.collider = entry.runtime.body->addCollider(entry.runtime.shape.shape, reactphysics3d::Transform::identity());
    if (entry.runtime.collider) {
        entry.runtime.collider->setUserData(reinterpret_cast<void *>(static_cast<std::uintptr_t>(entry.id)));
        // トリガーは物理シミュレーション（押し戻し）の対象から外し、すり抜けるようにする。
        // 一方でワールドクエリの対象からは外さないため、衝突検出に使っている
        // PhysicsWorld::testCollision には引き続き現れ、OnCollisionEnter/Stay/Exitは通知される。
        // （RP3Dのsetter名の通り「シミュレーション用の接触を生成するか」と
        // 「ワールドクエリの結果に含めるか」は別のフラグとして管理されている）
        entry.runtime.collider->setIsSimulationCollider(!entry.info.isTrigger);
        entry.runtime.collider->setIsWorldQueryCollider(true);
    }

    return entry.runtime.collider != nullptr;
}

void Collider::ReleaseRuntime3D(Entry<ColliderInfo3D, ColliderRuntime3D> &entry) {
    if (entry.runtime.body) {
        if (entry.runtime.collider) {
            entry.runtime.body->removeCollider(entry.runtime.collider);
        }
        if (physicsWorld_ && entry.runtime.ownsBody) {
            physicsWorld_->destroyRigidBody(entry.runtime.body);
        }
    }

    if (entry.runtime.shape.shape) {
        std::visit(
            [&](const auto &shape) {
                using S = std::decay_t<decltype(shape)>;
                if constexpr (std::is_same_v<S, ColliderInfo3D::SphereShape3D>) {
                    physicsCommon_.destroySphereShape(static_cast<reactphysics3d::SphereShape *>(entry.runtime.shape.shape));
                } else if constexpr (std::is_same_v<S, ColliderInfo3D::BoxShape3D>) {
                    physicsCommon_.destroyBoxShape(static_cast<reactphysics3d::BoxShape *>(entry.runtime.shape.shape));
                } else if constexpr (std::is_same_v<S, ColliderInfo3D::CapsuleShape3D>) {
                    physicsCommon_.destroyCapsuleShape(static_cast<reactphysics3d::CapsuleShape *>(entry.runtime.shape.shape));
                } else if constexpr (std::is_same_v<S, ColliderInfo3D::ConvexMeshShape3D>) {
                    physicsCommon_.destroyConvexMeshShape(static_cast<reactphysics3d::ConvexMeshShape *>(entry.runtime.shape.shape));
                    if (entry.runtime.shape.convexMesh) {
                        physicsCommon_.destroyConvexMesh(entry.runtime.shape.convexMesh);
                    }
                } else if constexpr (std::is_same_v<S, ColliderInfo3D::ConcaveMeshShape3D>) {
                    physicsCommon_.destroyConcaveMeshShape(static_cast<reactphysics3d::ConcaveMeshShape *>(entry.runtime.shape.shape));
                    if (entry.runtime.shape.triangleMesh) {
                        physicsCommon_.destroyTriangleMesh(entry.runtime.shape.triangleMesh);
                    }
                } else if constexpr (std::is_same_v<S, ColliderInfo3D::HeightFieldShape3D>) {
                    physicsCommon_.destroyHeightFieldShape(static_cast<reactphysics3d::HeightFieldShape *>(entry.runtime.shape.shape));
                    if (entry.runtime.shape.heightField) {
                        physicsCommon_.destroyHeightField(entry.runtime.shape.heightField);
                    }
                }
            },
            entry.info.shape);
    }

    entry.runtime = {};
}

bool Collider::UpdateRuntime3D(Entry<ColliderInfo3D, ColliderRuntime3D> &entry, const ColliderInfo3D &info) {
    ReleaseRuntime3D(entry);
    entry.info = info;
    return BuildRuntime3D(entry);
}

bool Collider::UpdateColliderShape3D(Entry<ColliderInfo3D, ColliderRuntime3D> &entry, const ColliderInfo3D &info) {
    return UpdateRuntime3D(entry, info);
}

bool Collider::UpdateColliderTransform3D(Entry<ColliderInfo3D, ColliderRuntime3D> &entry, const ColliderInfo3D &info) {
    if (!entry.runtime.body) return false;
    entry.runtime.body->setTransform(MakeTransform3D(info));
    return true;
}

std::optional<Collider::ShapeHandle3D> Collider::CreateShape3D(const ColliderInfo3D &info) {
    ShapeHandle3D handle{};

    std::visit(
        [&](const auto &shape) {
            using S = std::decay_t<decltype(shape)>;

            if constexpr (std::is_same_v<S, ColliderInfo3D::SphereShape3D>) {
                handle.shape = physicsCommon_.createSphereShape(shape.radius);
            } else if constexpr (std::is_same_v<S, ColliderInfo3D::BoxShape3D>) {
                handle.shape = physicsCommon_.createBoxShape(ToRp3d(shape.halfExtents));
            } else if constexpr (std::is_same_v<S, ColliderInfo3D::CapsuleShape3D>) {
                handle.shape = physicsCommon_.createCapsuleShape(shape.radius, shape.height);
            } else if constexpr (std::is_same_v<S, ColliderInfo3D::ConvexMeshShape3D>) {
                if (shape.vertices.empty()) return;

                constexpr std::uint32_t kVertexStride = sizeof(Vector3);
                // 描画用メッシュは面ごとに頂点が重複していることが多く、三角形インデックスを
                // PolygonVertexArrayとしてそのまま渡すと「閉じた凸メッシュ」として不正になり、
                // createConvexMesh()が失敗してランタイムColliderが生成されない場合がある。
                // 頂点群から凸包を再構築するVertexArray版を使うことで、描画メッシュのトポロジーに
                // 依存せず安定して物理用の凸形状を作成する。
                reactphysics3d::VertexArray array(
                    shape.vertices.data(),
                    kVertexStride,
                    static_cast<std::uint32_t>(shape.vertices.size()),
                    reactphysics3d::VertexArray::DataType::VERTEX_FLOAT_TYPE);
                std::vector<reactphysics3d::Message> messages;
                handle.convexMesh = physicsCommon_.createConvexMesh(array, messages);
                if (handle.convexMesh) {
                    handle.shape = physicsCommon_.createConvexMeshShape(handle.convexMesh, ToRp3d(shape.scale));
                }
            } else if constexpr (std::is_same_v<S, ColliderInfo3D::ConcaveMeshShape3D>) {
                // 非凸メッシュは描画用の三角形インデックスをそのまま使う。TriangleMeshは入力配列を
                // 内部へコピーして保持するため、このローカルのTriangleVertexArrayは生成後に破棄してよい。
                if (shape.vertices.empty() || shape.indices.empty() || shape.indices.size() % 3 != 0) return;
                constexpr std::uint32_t kVertexStride = sizeof(Vector3);
                constexpr std::uint32_t kIndexStride = sizeof(std::uint32_t);
                reactphysics3d::TriangleVertexArray array(
                    static_cast<std::uint32_t>(shape.vertices.size()),
                    shape.vertices.data(),
                    kVertexStride,
                    static_cast<std::uint32_t>(shape.indices.size() / 3),
                    shape.indices.data(),
                    kIndexStride * 3,
                    reactphysics3d::TriangleVertexArray::VertexDataType::VERTEX_FLOAT_TYPE,
                    reactphysics3d::TriangleVertexArray::IndexDataType::INDEX_INTEGER_TYPE);
                std::vector<reactphysics3d::Message> messages;
                handle.triangleMesh = physicsCommon_.createTriangleMesh(array, messages);
                if (!handle.triangleMesh) return;

                handle.concaveMesh = physicsCommon_.createConcaveMeshShape(handle.triangleMesh, ToRp3d(shape.scale));
                if (handle.concaveMesh) {
                    handle.shape = handle.concaveMesh;
                } else {
                    physicsCommon_.destroyTriangleMesh(handle.triangleMesh);
                    handle.triangleMesh = nullptr;
                }
            } else if constexpr (std::is_same_v<S, ColliderInfo3D::HeightFieldShape3D>) {
                if (shape.heights.empty() || shape.width == 0 || shape.length == 0) return;
                std::vector<reactphysics3d::Message> messages;
                handle.heightField = physicsCommon_.createHeightField(
                    shape.width,
                    shape.length,
                    shape.heights.data(),
                    rp3d::HeightField::HeightDataType::HEIGHT_FLOAT_TYPE,
                    messages,
                    shape.scale.y);
                if (handle.heightField) {
                    handle.shape = physicsCommon_.createHeightFieldShape(handle.heightField, ToRp3d(shape.scale));
                }
            }
        },
        info.shape);

    if (!handle.shape) return std::nullopt;
    return handle;
}

reactphysics3d::Transform Collider::MakeTransform3D(const ColliderInfo3D &info) const {
    Vector3 center{0.0f, 0.0f, 0.0f};
    bool hasOwnCenter = false;
    std::visit(
        [&](const auto &shape) {
            using S = std::decay_t<decltype(shape)>;
            if constexpr (std::is_same_v<S, ColliderInfo3D::SphereShape3D> ||
                          std::is_same_v<S, ColliderInfo3D::BoxShape3D> ||
                          std::is_same_v<S, ColliderInfo3D::CapsuleShape3D>) {
                center = shape.center;
                hasOwnCenter = true;
            }
        },
        info.shape);

    // ConvexMeshShape3D/ConcaveMeshShape3D/HeightFieldShape3Dは形状側に位置を持たない
    // （頂点はオブジェクトのローカル座標系のまま）ため、コライダーの同期設定を考慮した位置を使用する
    if (!hasOwnCenter && info.sourceCollider) {
        center = info.sourceCollider->GetSyncedOwnerPosition();
    }

    reactphysics3d::Quaternion rotation = reactphysics3d::Quaternion::identity();
    if (info.sourceCollider) {
        const auto rot = info.sourceCollider->GetSyncedOwnerRotation();
        rotation = reactphysics3d::Quaternion(rot.x, rot.y, rot.z, rot.w);
    }
    return reactphysics3d::Transform(ToRp3d(center), rotation);
}

reactphysics3d::Vector3 Collider::ToRp3d(const Vector3 &v) const {
    return reactphysics3d::Vector3(v.x, v.y, v.z);
}

Vector3 Collider::FromRp3d(const reactphysics3d::Vector3 &v) const {
    return Vector3{v.x, v.y, v.z};
}

HitInfo3D Collider::BuildHitInfo3D(const reactphysics3d::CollisionCallback::ContactPoint &contact) const {
    HitInfo3D info;
    info.isHit = true;
    info.normal = FromRp3d(contact.getWorldNormal());
    info.penetration = contact.getPenetrationDepth();
    return info;
}

} // namespace KashipanEngine
