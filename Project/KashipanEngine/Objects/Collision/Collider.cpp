#include "Collider.h"

#include "Objects/EmptyObject.h"
#include "Objects/Collision/CollisionAlgorithms2D.h"
#include "Objects/Components/Transform.h"
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
struct Bounds2D {
    float minX = 0.0f;
    float minY = 0.0f;
    float maxX = 0.0f;
    float maxY = 0.0f;
};

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

std::optional<Bounds2D> ComputeBounds2D(const ColliderInfo2D::ShapeVariant &shape) {
    return std::visit(
        [](const auto &s) -> std::optional<Bounds2D> {
            using S = std::decay_t<decltype(s)>;

            if constexpr (std::is_same_v<S, Math::Point2D>) {
                return Bounds2D{s.position.x, s.position.y, s.position.x, s.position.y};
            } else if constexpr (std::is_same_v<S, Math::Circle>) {
                return Bounds2D{s.center.x - s.radius, s.center.y - s.radius, s.center.x + s.radius, s.center.y + s.radius};
            } else if constexpr (std::is_same_v<S, Math::Rect>) {
                if (s.rotation == 0.0f) {
                    return Bounds2D{s.center.x - s.halfSize.x, s.center.y - s.halfSize.y, s.center.x + s.halfSize.x, s.center.y + s.halfSize.y};
                }
                // 回転している場合は4頂点を実際に回転させ、それを包むAABBを求める
                const float c = std::cos(s.rotation);
                const float sn = std::sin(s.rotation);
                const Vector2 ex{s.halfSize.x * c, s.halfSize.x * sn};
                const Vector2 ey{-s.halfSize.y * sn, s.halfSize.y * c};
                const Vector2 corners[4] = {
                    s.center - ex - ey, s.center + ex - ey,
                    s.center + ex + ey, s.center - ex + ey};
                Vector2 minV = corners[0];
                Vector2 maxV = corners[0];
                for (int i = 1; i < 4; ++i) {
                    minV.x = std::min(minV.x, corners[i].x);
                    minV.y = std::min(minV.y, corners[i].y);
                    maxV.x = std::max(maxV.x, corners[i].x);
                    maxV.y = std::max(maxV.y, corners[i].y);
                }
                return Bounds2D{minV.x, minV.y, maxV.x, maxV.y};
            } else if constexpr (std::is_same_v<S, Math::Segment2D>) {
                return Bounds2D{
                    std::min(s.start.x, s.end.x),
                    std::min(s.start.y, s.end.y),
                    std::max(s.start.x, s.end.x),
                    std::max(s.start.y, s.end.y)};
            } else if constexpr (std::is_same_v<S, Math::Capsule2D>) {
                return Bounds2D{
                    std::min(s.start.x, s.end.x) - s.radius,
                    std::min(s.start.y, s.end.y) - s.radius,
                    std::max(s.start.x, s.end.x) + s.radius,
                    std::max(s.start.y, s.end.y) + s.radius};
            } else {
                return std::nullopt;
            }
        },
        shape);
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
std::vector<IndexPair> BuildCandidatePairs2D(const TColliders &colliders) {
    constexpr float kCellSize = 10.0f;
    constexpr int kMaxCellsPerShape = 256;

    std::unordered_map<std::uint64_t, std::vector<std::size_t>> grid;
    std::vector<std::size_t> active;
    std::vector<std::size_t> global;

    active.reserve(colliders.size());
    global.reserve(colliders.size());

    for (std::size_t i = 0; i < colliders.size(); ++i) {
        const auto &c = colliders[i];
        if (!c.info.enabled) continue;
        active.push_back(i);

        const auto bounds = ComputeBounds2D(c.info.shape);
        if (!bounds.has_value()) {
            global.push_back(i);
            continue;
        }

        const int minX = ToCell(bounds->minX, kCellSize);
        const int minY = ToCell(bounds->minY, kCellSize);
        const int maxX = ToCell(bounds->maxX, kCellSize);
        const int maxY = ToCell(bounds->maxY, kCellSize);

        const int cellsX = (maxX - minX + 1);
        const int cellsY = (maxY - minY + 1);
        if (cellsX <= 0 || cellsY <= 0 || cellsX > kMaxCellsPerShape || cellsY > kMaxCellsPerShape || (cellsX * cellsY) > kMaxCellsPerShape) {
            global.push_back(i);
            continue;
        }

        for (int y = minY; y <= maxY; ++y) {
            for (int x = minX; x <= maxX; ++x) {
                const std::uint64_t key = (static_cast<std::uint64_t>(static_cast<std::uint32_t>(x)) << 32)
                                        | static_cast<std::uint32_t>(y);
                grid[key].push_back(i);
            }
        }
    }

    std::unordered_set<IndexPair, IndexPairHash> uniquePairs;

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
    for (const auto &p : uniquePairs) out.push_back(p);
    return out;
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

inline bool Intersects2D(const ColliderInfo2D::ShapeVariant &a, const ColliderInfo2D::ShapeVariant &b) {
    return std::visit(
        [](const auto &lhs, const auto &rhs) {
            if constexpr (requires { KashipanEngine::CollisionAlgorithms2D::Intersects(lhs, rhs); }) {
                return KashipanEngine::CollisionAlgorithms2D::Intersects(lhs, rhs);
            } else {
                return false;
            }
        },
        a, b);
}

inline bool Intersects3D(const ColliderInfo3D::ShapeVariant &a, const ColliderInfo3D::ShapeVariant &b) {
    (void)a;
    (void)b;
    return false;
}

inline HitInfo2D ComputeHit2D(const ColliderInfo2D::ShapeVariant &a, const ColliderInfo2D::ShapeVariant &b) {
    return std::visit(
        [](const auto &lhs, const auto &rhs) -> HitInfo2D {
            if constexpr (requires { KashipanEngine::CollisionAlgorithms2D::ComputeHit(lhs, rhs); }) {
                HitInfo hiBase = KashipanEngine::CollisionAlgorithms2D::ComputeHit(lhs, rhs);
                HitInfo2D hi;
                hi.isHit = hiBase.isHit;
                hi.normal = hiBase.normal;
                hi.penetration = hiBase.penetration;
                return hi;
            } else if constexpr (requires { KashipanEngine::CollisionAlgorithms2D::Intersects(lhs, rhs); }) {
                HitInfo2D hi;
                hi.isHit = KashipanEngine::CollisionAlgorithms2D::Intersects(lhs, rhs);
                return hi;
            } else {
                return HitInfo2D{};
            }
        },
        a, b);
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

/// @brief 2D形状の最小半径（この距離以下の移動ならすり抜けは起きないとみなせる量）を求める
float ComputeMinHalfExtent2D(const ColliderInfo2D::ShapeVariant &shape) {
    const auto bounds = ComputeBounds2D(shape);
    if (!bounds) return kMinSweepStep;
    const float width = bounds->maxX - bounds->minX;
    const float height = bounds->maxY - bounds->minY;
    return std::max(kMinSweepStep, std::min(width, height) * 0.5f);
}

/// @brief 3D形状の最小半径を求める
float ComputeMinHalfExtent3D(const ColliderInfo3D::ShapeVariant &shape) {
    const auto bounds = ComputeBounds3D(shape);
    if (!bounds) return kMinSweepStep;
    const Vector3 size = bounds->max - bounds->min;
    return std::max(kMinSweepStep, std::min({ size.x, size.y, size.z }) * 0.5f);
}

/// @brief 2D形状を平行移動した複製を返す（スイープの中間位置の判定用）
ColliderInfo2D::ShapeVariant TranslateShape2D(const ColliderInfo2D::ShapeVariant &shape, const Vector2 &offset) {
    ColliderInfo2D::ShapeVariant result = shape;
    std::visit(
        [&](auto &s) {
            using S = std::decay_t<decltype(s)>;
            if constexpr (std::is_same_v<S, Math::Point2D>) {
                s.position += offset;
            } else if constexpr (std::is_same_v<S, Math::Circle>) {
                s.center += offset;
            } else if constexpr (std::is_same_v<S, Math::Rect>) {
                s.center += offset;
            } else if constexpr (std::is_same_v<S, Math::Segment2D>) {
                s.start += offset;
                s.end += offset;
            } else if constexpr (std::is_same_v<S, Math::Capsule2D>) {
                s.start += offset;
                s.end += offset;
            }
        },
        result);
    return result;
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
    return EraseById(colliders2D_, id);
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
        if (e.id == id) {
            e.info = info;
            return true;
        }
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

std::vector<Collider::HitPair2D> Collider::CheckAll2D() const {
    std::vector<HitPair2D> hits;
    const auto pairs = BuildCandidatePairs2D(colliders2D_);
    hits.reserve(pairs.size());

    for (const auto &pair : pairs) {
        const auto &ai = colliders2D_[pair.a];
        const auto &bi = colliders2D_[pair.b];

        if (!ai.info.enabled || !bi.info.enabled) continue;
        if (!ShouldTest(ai.info.attribute, ai.info.ignoreAttribute, bi.info.attribute) ||
            !ShouldTest(bi.info.attribute, bi.info.ignoreAttribute, ai.info.attribute)) {
            continue;
        }

        if (Intersects2D(ai.info.shape, bi.info.shape)) {
            hits.push_back({ai.id, bi.id});
        }
    }
    return hits;
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

bool Collider::Check2D(ColliderID a, ColliderID b) const {
    const auto *pa = Find2D(a);
    const auto *pb = Find2D(b);
    if (!pa || !pb) return false;
    if (!pa->info.enabled || !pb->info.enabled) return false;

    if (!ShouldTest(pa->info.attribute, pa->info.ignoreAttribute, pb->info.attribute) ||
        !ShouldTest(pb->info.attribute, pb->info.ignoreAttribute, pa->info.attribute)) {
        return false;
    }

    return Intersects2D(pa->info.shape, pb->info.shape);
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

void Collider::Update2D() {
    std::vector<std::uint64_t> cur;

    // 連続衝突判定（スイープ）でのみ検出できるヒットを先に収集しておく
    // （現在位置で重なっているペアは通常判定のヒット情報を優先する）
    std::unordered_map<std::uint64_t, HitInfo2D> continuousHits;
    CollectContinuousHits2D(continuousHits);

    const auto pairs = BuildCandidatePairs2D(colliders2D_);
    cur.reserve(pairs.size());

    // このフレームでDispatch済みのペア（Exitの発火漏れ・二重発火防止用）
    std::vector<std::uint64_t> processedPairs;
    processedPairs.reserve(pairs.size());

    for (const auto &pair : pairs) {
        const auto &ai = colliders2D_[pair.a];
        const auto &bi = colliders2D_[pair.b];

        if (!ai.info.enabled || !bi.info.enabled) continue;
        if (!ShouldTest(ai.info.attribute, ai.info.ignoreAttribute, bi.info.attribute) ||
            !ShouldTest(bi.info.attribute, bi.info.ignoreAttribute, ai.info.attribute)) {
            continue;
        }

        HitInfo2D hi = ComputeHit2D(ai.info.shape, bi.info.shape);
        const std::uint64_t key = MakePairKey(ai.id, bi.id);

        // 現在位置で重なっていなくても、スイープで通過が検出されていればヒット扱いにする
        if (!hi.isHit) {
            auto ccdIt = continuousHits.find(key);
            if (ccdIt != continuousHits.end()) {
                hi = ccdIt->second;
                // スイープのHitInfoは小さいID側を自分として計算されているため、順序が逆なら法線を反転する
                if (ai.id > bi.id) hi.normal = -hi.normal;
            }
        }
        continuousHits.erase(key);

        const bool wasHit = std::binary_search(prevPairs2D_.begin(), prevPairs2D_.end(), key);
        Dispatch2D(ai.id, bi.id, hi, wasHit);
        processedPairs.push_back(key);

        if (hi.isHit) cur.push_back(key);
    }

    // 候補ペアに挙がらなかったスイープヒット（高速移動で現在位置が既に離れている場合）を発火する
    for (const auto &[key, hi] : continuousHits) {
        const ColliderID a = static_cast<ColliderID>(key >> 32);
        const ColliderID b = static_cast<ColliderID>(key & 0xffffffffu);
        const bool wasHit = std::binary_search(prevPairs2D_.begin(), prevPairs2D_.end(), key);
        Dispatch2D(a, b, hi, wasHit);
        processedPairs.push_back(key);
        cur.push_back(key);
    }

    // 前フレームは接触していたが、今フレームの候補に挙がらなかった（高速に離れた）ペアのExitを発火する
    std::sort(processedPairs.begin(), processedPairs.end());
    for (const auto key : prevPairs2D_) {
        if (std::binary_search(processedPairs.begin(), processedPairs.end(), key)) continue;
        const ColliderID a = static_cast<ColliderID>(key >> 32);
        const ColliderID b = static_cast<ColliderID>(key & 0xffffffffu);
        Dispatch2D(a, b, HitInfo2D{}, true);
    }

    std::sort(cur.begin(), cur.end());
    prevPairs2D_ = std::move(cur);

    RecordPrevPositions2D();
}

void Collider::CollectContinuousHits2D(std::unordered_map<std::uint64_t, HitInfo2D> &outHits) {
    for (auto &entry : colliders2D_) {
        if (!entry.info.continuousDetection || !entry.info.enabled) continue;
        if (!entry.hasPrevPosition) continue;

        const auto bounds = ComputeBounds2D(entry.info.shape);
        if (!bounds) continue;
        const Vector2 currentCenter((bounds->minX + bounds->maxX) * 0.5f, (bounds->minY + bounds->maxY) * 0.5f);
        const Vector2 prevCenter(entry.prevPosition.x, entry.prevPosition.y);
        const Vector2 delta = currentCenter - prevCenter;
        const float distance = delta.Length();

        // 1フレームの移動量が形状の最小半径以下なら、すり抜けは起きない（通常判定で検出できる）
        const float maxStep = ComputeMinHalfExtent2D(entry.info.shape);
        if (distance <= maxStep) continue;

        const int substeps = std::min(kMaxSweepSubsteps, static_cast<int>(std::ceil(distance / maxStep)));

        for (const auto &other : colliders2D_) {
            if (other.id == entry.id || !other.info.enabled) continue;
            if (!ShouldTest(entry.info.attribute, entry.info.ignoreAttribute, other.info.attribute) ||
                !ShouldTest(other.info.attribute, other.info.ignoreAttribute, entry.info.attribute)) {
                continue;
            }
            const std::uint64_t key = MakePairKey(entry.id, other.id);
            if (outHits.contains(key)) continue; // 両方CCDの場合の二重スイープを防ぐ

            // 移動経路の中間位置（終端は通常判定が受け持つ）を時刻順に判定し、最初のヒットを採用する
            for (int i = 1; i < substeps; ++i) {
                const float t = static_cast<float>(i) / static_cast<float>(substeps);
                // 中間位置 = prev + delta*t。現在形状からの相対移動量へ変換して平行移動する
                const Vector2 offset = delta * (t - 1.0f);
                const auto sweptShape = TranslateShape2D(entry.info.shape, offset);
                // ComputeHitは(自分, 相手)の順で「相手→自分」向きの法線を返すため、
                // Dispatch側がID昇順で解釈できるよう小さいID側を自分として計算する
                const HitInfo2D hit = (entry.id < other.id)
                    ? ComputeHit2D(sweptShape, other.info.shape)
                    : ComputeHit2D(other.info.shape, sweptShape);
                if (hit.isHit) {
                    outHits.emplace(key, hit);
                    break;
                }
            }
        }
    }
}

void Collider::RecordPrevPositions2D() {
    for (auto &entry : colliders2D_) {
        entry.hasPrevPosition = false;
        if (!entry.info.continuousDetection || !entry.info.enabled) continue;
        const auto bounds = ComputeBounds2D(entry.info.shape);
        if (!bounds) continue;
        entry.prevPosition = Vector3((bounds->minX + bounds->maxX) * 0.5f, (bounds->minY + bounds->maxY) * 0.5f, 0.0f);
        entry.hasPrevPosition = true;
    }
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
    accumulatedTime_ += deltaTime;

    while (accumulatedTime_ >= kDefaultTimeStep) {
        StepPhysics(kDefaultTimeStep);
        accumulatedTime_ -= kDefaultTimeStep;
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

const Collider::Entry<ColliderInfo2D> *Collider::Find2D(ColliderID id) const {
    for (const auto &e : colliders2D_) {
        if (e.id == id) return &e;
    }
    return nullptr;
}

const Collider::Entry<ColliderInfo3D> *Collider::Find3D(ColliderID id) const {
    for (const auto &e : colliders3D_) {
        if (e.id == id) return &e;
    }
    return nullptr;
}

Collider::Entry<ColliderInfo3D> *Collider::Find3D(ColliderID id) {
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
    if (physicsWorld_) return;

    reactphysics3d::PhysicsWorld::WorldSettings settings;
    settings.gravity = reactphysics3d::Vector3(0.0f, -9.81f, 0.0f);
    physicsWorld_ = physicsCommon_.createPhysicsWorld(settings);
}

void Collider::ReleaseWorld() {
    if (physicsWorld_) {
        physicsCommon_.destroyPhysicsWorld(physicsWorld_);
        physicsWorld_ = nullptr;
    }
}

bool Collider::BuildRuntime3D(Entry<ColliderInfo3D> &entry) {
    EnsureWorldCreated();
    if (!physicsWorld_) return false;

    ReleaseRuntime3D(entry);

    auto shapeHandle = CreateShape3D(entry.info);
    if (!shapeHandle.has_value()) return false;

    entry.runtime.shape = shapeHandle.value();
    const auto transform = MakeTransform3D(entry.info);
    // RigidBody3Dが使用コライダーを明示的に選択している場合は、そのコライダーだけを
    // RigidBodyへ取り付ける（未選択の場合は従来通りどのコライダーでも取り付ける）
    auto *rb = entry.info.ownerObject->GetComponent<RigidBody3D>();
    if (rb) {
        auto *selected = rb->GetSelectedCollider();
        if (selected && selected != entry.info.sourceCollider) rb = nullptr;
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

void Collider::ReleaseRuntime3D(Entry<ColliderInfo3D> &entry) {
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

bool Collider::UpdateRuntime3D(Entry<ColliderInfo3D> &entry, const ColliderInfo3D &info) {
    ReleaseRuntime3D(entry);
    entry.info = info;
    return BuildRuntime3D(entry);
}

bool Collider::UpdateColliderShape3D(Entry<ColliderInfo3D> &entry, const ColliderInfo3D &info) {
    return UpdateRuntime3D(entry, info);
}

bool Collider::UpdateColliderTransform3D(Entry<ColliderInfo3D> &entry, const ColliderInfo3D &info) {
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
                if (shape.vertices.empty() || shape.indices.empty()) return;

                constexpr std::uint32_t kVertexStride = sizeof(Vector3);
                constexpr std::uint32_t kIndexStride = sizeof(std::uint32_t);
                const std::uint32_t polygonCount = static_cast<std::uint32_t>(shape.indices.size() / 3);

                std::vector<reactphysics3d::PolygonVertexArray::PolygonFace> faces;
                faces.resize(polygonCount);
                for (std::uint32_t i = 0; i < polygonCount; ++i) {
                    faces[i].indexBase = i * 3;
                    faces[i].nbVertices = 3;
                }

                reactphysics3d::PolygonVertexArray array(
                    static_cast<std::uint32_t>(shape.vertices.size()),
                    reinterpret_cast<const reactphysics3d::Vector3 *>(shape.vertices.data()),
                    kVertexStride,
                    shape.indices.data(),
                    kIndexStride,
                    polygonCount,
                    faces.data(),
                    reactphysics3d::PolygonVertexArray::VertexDataType::VERTEX_FLOAT_TYPE,
                    reactphysics3d::PolygonVertexArray::IndexDataType::INDEX_INTEGER_TYPE);

                std::vector<reactphysics3d::Message> messages;
                handle.convexMesh = physicsCommon_.createConvexMesh(array, messages);
                if (handle.convexMesh) {
                    handle.shape = physicsCommon_.createConvexMeshShape(handle.convexMesh, ToRp3d(shape.scale));
                }
            } else if constexpr (std::is_same_v<S, ColliderInfo3D::ConcaveMeshShape3D>) {
                /*if (shape.vertices.empty() || shape.indices.empty()) return;
                constexpr std::uint32_t kVertexStride = sizeof(Vector3);
                constexpr std::uint32_t kIndexStride = sizeof(std::uint32_t);
                reactphysics3d::TriangleVertexArray array(
                    static_cast<std::uint32_t>(shape.vertices.size() / 3),
                    shape.vertices.data(),
                    kVertexStride * 3,
                    static_cast<std::uint32_t>(shape.indices.size() / 3),
                    shape.indices.data(),
                    kIndexStride * 3,
                    reactphysics3d::TriangleVertexArray::VertexDataType::VERTEX_FLOAT_TYPE,
                    reactphysics3d::TriangleVertexArray::IndexDataType::INDEX_INTEGER_TYPE);
                std::vector<reactphysics3d::Message> messages;
                reactphysics3d::TriangleMesh *mesh = physicsCommon_.createTriangleMesh(array, messages);
                handle.concaveMesh = physicsCommon_.createConcaveMeshShape(mesh, ToRp3d(shape.scale));
                if (handle.concaveMesh) {
                    handle.shape = handle.concaveMesh;
                }*/
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
