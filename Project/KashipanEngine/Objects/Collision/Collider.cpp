#include "Collider.h"

#include "Objects/Collision/CollisionAlgorithms2D.h"
#include "Objects/Collision/CollisionAlgorithms3D.h"

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

inline Bounds2D MergeBounds2D(const Bounds2D &a, const Bounds2D &b) {
    return Bounds2D{
        std::min(a.minX, b.minX),
        std::min(a.minY, b.minY),
        std::max(a.maxX, b.maxX),
        std::max(a.maxY, b.maxY)};
}

inline Vector2 BoundsCenter2D(const Bounds2D &b) {
    return Vector2{(b.minX + b.maxX) * 0.5f, (b.minY + b.maxY) * 0.5f};
}

inline Vector3 BoundsCenter3D(const Bounds3D &b) {
    return Vector3{
        (b.min.x + b.max.x) * 0.5f,
        (b.min.y + b.max.y) * 0.5f,
        (b.min.z + b.max.z) * 0.5f};
}

inline float BoundsMaxSize2D(const Bounds2D &b) {
    return std::max(b.maxX - b.minX, b.maxY - b.minY);
}

inline float BoundsMaxSize3D(const Bounds3D &b) {
    const float sx = b.max.x - b.min.x;
    const float sy = b.max.y - b.min.y;
    const float sz = b.max.z - b.min.z;
    return std::max({sx, sy, sz});
}

inline Bounds3D MergeBounds3D(const Bounds3D &a, const Bounds3D &b) {
    return Bounds3D{
        Vector3{
            std::min(a.min.x, b.min.x),
            std::min(a.min.y, b.min.y),
            std::min(a.min.z, b.min.z)},
        Vector3{
            std::max(a.max.x, b.max.x),
            std::max(a.max.y, b.max.y),
            std::max(a.max.z, b.max.z)}};
}

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
                return Bounds2D{s.center.x - s.halfSize.x, s.center.y - s.halfSize.y, s.center.x + s.halfSize.x, s.center.y + s.halfSize.y};
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

            if constexpr (std::is_same_v<S, Math::Point3D>) {
                return Bounds3D{s.position, s.position};
            } else if constexpr (std::is_same_v<S, Math::Sphere>) {
                const Vector3 r{s.radius, s.radius, s.radius};
                return Bounds3D{s.center - r, s.center + r};
            } else if constexpr (std::is_same_v<S, Math::AABB>) {
                return Bounds3D{s.min, s.max};
            } else if constexpr (std::is_same_v<S, Math::OBB>) {
                const float ex = std::abs(s.orientation.m[0][0]) * s.halfSize.x
                               + std::abs(s.orientation.m[1][0]) * s.halfSize.y
                               + std::abs(s.orientation.m[2][0]) * s.halfSize.z;
                const float ey = std::abs(s.orientation.m[0][1]) * s.halfSize.x
                               + std::abs(s.orientation.m[1][1]) * s.halfSize.y
                               + std::abs(s.orientation.m[2][1]) * s.halfSize.z;
                const float ez = std::abs(s.orientation.m[0][2]) * s.halfSize.x
                               + std::abs(s.orientation.m[1][2]) * s.halfSize.y
                               + std::abs(s.orientation.m[2][2]) * s.halfSize.z;

                const Vector3 ext{ex, ey, ez};
                return Bounds3D{s.center - ext, s.center + ext};
            } else if constexpr (std::is_same_v<S, Math::Plane>) {
                // 無限形状は空間分割に載せない（全体候補として扱う）
                return std::nullopt;
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

        std::optional<Bounds2D> bounds = ComputeBounds2D(c.info.shape);

        if (c.info.ccdEnabled && c.hasPrevShape) {
            const auto prevBounds = ComputeBounds2D(c.prevShape);
            if (bounds.has_value() && prevBounds.has_value()) {
                bounds = MergeBounds2D(*bounds, *prevBounds);
            } else if (!bounds.has_value() && prevBounds.has_value()) {
                bounds = prevBounds;
            }
        }

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

inline std::uint64_t MakeCellKey3D(int x, int y, int z) {
    const std::uint64_t ux = static_cast<std::uint32_t>(x);
    const std::uint64_t uy = static_cast<std::uint32_t>(y);
    const std::uint64_t uz = static_cast<std::uint32_t>(z);

    std::uint64_t h = 0x9e3779b97f4a7c15ull;
    h ^= ux + 0x9e3779b97f4a7c15ull + (h << 6) + (h >> 2);
    h ^= uy + 0x9e3779b97f4a7c15ull + (h << 6) + (h >> 2);
    h ^= uz + 0x9e3779b97f4a7c15ull + (h << 6) + (h >> 2);
    return h;
}

inline std::uint64_t MakeIndexPairKey(std::size_t a, std::size_t b) {
    if (a > b) std::swap(a, b);
    return (static_cast<std::uint64_t>(static_cast<std::uint32_t>(a)) << 32)
         | static_cast<std::uint32_t>(b);
}

template<typename TColliders>
const std::vector<IndexPair> &BuildCandidatePairs3D(const TColliders &colliders) {
    constexpr float kCellSize = 16.0f;
    constexpr int kMaxCellsPerShape = 512;

    struct BuildCache3D {
        std::unordered_map<std::uint64_t, std::vector<std::size_t>> grid;
        std::vector<std::uint64_t> usedCells;
        std::vector<std::size_t> active;
        std::vector<std::size_t> global;
        std::unordered_set<std::uint64_t> uniquePairKeys;
        std::vector<IndexPair> pairs;
    };

    thread_local BuildCache3D cache;

    auto &grid = cache.grid;
    auto &usedCells = cache.usedCells;
    auto &active = cache.active;
    auto &global = cache.global;
    auto &uniquePairKeys = cache.uniquePairKeys;
    auto &pairs = cache.pairs;

    for (auto &it : grid) {
        it.second.clear();
    }

    usedCells.clear();
    active.clear();
    global.clear();
    uniquePairKeys.clear();
    pairs.clear();

    if (active.capacity() < colliders.size()) {
        active.reserve(colliders.size());
    }
    if (global.capacity() < colliders.size()) {
        global.reserve(colliders.size());
    }

    for (std::size_t i = 0; i < colliders.size(); ++i) {
        const auto &c = colliders[i];
        if (!c.info.enabled) continue;
        active.push_back(i);

        std::optional<Bounds3D> bounds = ComputeBounds3D(c.info.shape);

        if (c.info.ccdEnabled && c.hasPrevShape) {
            const auto prevBounds = ComputeBounds3D(c.prevShape);
            if (bounds.has_value() && prevBounds.has_value()) {
                bounds = MergeBounds3D(*bounds, *prevBounds);
            } else if (!bounds.has_value() && prevBounds.has_value()) {
                bounds = prevBounds;
            }
        }

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

        const std::int64_t cellCount = static_cast<std::int64_t>(cellsX) * static_cast<std::int64_t>(cellsY) * static_cast<std::int64_t>(cellsZ);
        if (cellsX <= 0 || cellsY <= 0 || cellsZ <= 0 || cellsX > kMaxCellsPerShape || cellsY > kMaxCellsPerShape || cellsZ > kMaxCellsPerShape || cellCount > kMaxCellsPerShape) {
            global.push_back(i);
            continue;
        }

        for (int z = minZ; z <= maxZ; ++z) {
            for (int y = minY; y <= maxY; ++y) {
                for (int x = minX; x <= maxX; ++x) {
                    const std::uint64_t key = MakeCellKey3D(x, y, z);
                    auto &cell = grid[key];
                    if (cell.empty()) {
                        usedCells.push_back(key);
                    }
                    cell.push_back(i);
                }
            }
        }
    }

    uniquePairKeys.reserve((usedCells.size() * 2) + (global.size() * active.size()));

    for (std::uint64_t cellKey : usedCells) {
        const auto it = grid.find(cellKey);
        if (it == grid.end()) continue;

        const auto &indices = it->second;
        for (std::size_t i = 0; i < indices.size(); ++i) {
            for (std::size_t j = i + 1; j < indices.size(); ++j) {
                uniquePairKeys.insert(MakeIndexPairKey(indices[i], indices[j]));
            }
        }
    }

    for (std::size_t g : global) {
        for (std::size_t a : active) {
            if (a == g) continue;
            uniquePairKeys.insert(MakeIndexPairKey(g, a));
        }
    }

    pairs.reserve(uniquePairKeys.size());
    for (std::uint64_t k : uniquePairKeys) {
        pairs.push_back(IndexPair{
            static_cast<std::size_t>(static_cast<std::uint32_t>(k >> 32)),
            static_cast<std::size_t>(static_cast<std::uint32_t>(k & 0xffffffffull))});
    }

    return pairs;
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

inline int ComputeAdaptiveSamples2D(
    const ColliderInfo2D::ShapeVariant &currentA,
    const ColliderInfo2D::ShapeVariant *prevA,
    bool ccdA,
    const ColliderInfo2D::ShapeVariant &currentB,
    const ColliderInfo2D::ShapeVariant *prevB,
    bool ccdB) {
    float maxTravel = 0.0f;
    float minFeature = std::numeric_limits<float>::max();

    const auto accumulate = [&](const ColliderInfo2D::ShapeVariant &current,
                                const ColliderInfo2D::ShapeVariant *prev,
                                bool ccd) {
        const auto curBounds = ComputeBounds2D(current);
        if (curBounds.has_value()) {
            minFeature = std::min(minFeature, std::max(0.001f, BoundsMaxSize2D(*curBounds)));
        }

        if (!ccd || !prev) return;

        const auto prevBounds = ComputeBounds2D(*prev);
        if (!curBounds.has_value() || !prevBounds.has_value()) return;

        const float travel = BoundsCenter2D(*curBounds).Distance(BoundsCenter2D(*prevBounds));
        maxTravel = std::max(maxTravel, travel);
    };

    accumulate(currentA, prevA, ccdA);
    accumulate(currentB, prevB, ccdB);

    if (!std::isfinite(minFeature) || minFeature <= 0.0f) {
        minFeature = 1.0f;
    }

    const float stepDistance = std::max(0.05f, minFeature * 0.5f);
    const int travelSteps = static_cast<int>(std::ceil(maxTravel / stepDistance));
    return std::clamp(4 + travelSteps, 8, 96);
}

inline int ComputeAdaptiveSamples3D(
    const ColliderInfo3D::ShapeVariant &currentA,
    const ColliderInfo3D::ShapeVariant *prevA,
    bool ccdA,
    const ColliderInfo3D::ShapeVariant &currentB,
    const ColliderInfo3D::ShapeVariant *prevB,
    bool ccdB) {
    float maxTravel = 0.0f;
    float minFeature = std::numeric_limits<float>::max();

    const auto accumulate = [&](const ColliderInfo3D::ShapeVariant &current,
                                const ColliderInfo3D::ShapeVariant *prev,
                                bool ccd) {
        const auto curBounds = ComputeBounds3D(current);
        if (curBounds.has_value()) {
            minFeature = std::min(minFeature, std::max(0.001f, BoundsMaxSize3D(*curBounds)));
        }

        if (!ccd || !prev) return;

        const auto prevBounds = ComputeBounds3D(*prev);
        if (!curBounds.has_value() || !prevBounds.has_value()) return;

        const float travel = BoundsCenter3D(*curBounds).Distance(BoundsCenter3D(*prevBounds));
        maxTravel = std::max(maxTravel, travel);
    };

    accumulate(currentA, prevA, ccdA);
    accumulate(currentB, prevB, ccdB);

    if (!std::isfinite(minFeature) || minFeature <= 0.0f) {
        minFeature = 1.0f;
    }

    const float stepDistance = std::max(0.05f, minFeature * 0.5f);
    const int travelSteps = static_cast<int>(std::ceil(maxTravel / stepDistance));
    return std::clamp(4 + travelSteps, 8, 96);
}

inline bool Intersects3D(const ColliderInfo3D::ShapeVariant &a, const ColliderInfo3D::ShapeVariant &b) {
    return std::visit(
        [](const auto &lhs, const auto &rhs) {
            if constexpr (requires { KashipanEngine::CollisionAlgorithms3D::Intersects(lhs, rhs); }) {
                return KashipanEngine::CollisionAlgorithms3D::Intersects(lhs, rhs);
            } else {
                return false;
            }
        },
        a, b);
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
    return std::visit(
        [](const auto &lhs, const auto &rhs) -> HitInfo3D {
            if constexpr (requires { KashipanEngine::CollisionAlgorithms3D::ComputeHit(lhs, rhs); }) {
                HitInfo hiBase = KashipanEngine::CollisionAlgorithms3D::ComputeHit(lhs, rhs);
                HitInfo3D hi;
                hi.isHit = hiBase.isHit;
                hi.normal = hiBase.normal;
                hi.penetration = hiBase.penetration;
                return hi;
            } else if constexpr (requires { KashipanEngine::CollisionAlgorithms3D::Intersects(lhs, rhs); }) {
                HitInfo3D hi;
                hi.isHit = KashipanEngine::CollisionAlgorithms3D::Intersects(lhs, rhs);
                return hi;
            } else {
                return HitInfo3D{};
            }
        },
        a, b);
}

inline float LerpFloat(float a, float b, float t) {
    return a + (b - a) * t;
}

inline Vector2 LerpVec2(const Vector2 &a, const Vector2 &b, float t) {
    return a + (b - a) * t;
}

inline Vector3 LerpVec3(const Vector3 &a, const Vector3 &b, float t) {
    return a + (b - a) * t;
}

inline ColliderInfo2D::ShapeVariant InterpolateShape2D(
    const ColliderInfo2D::ShapeVariant &from,
    const ColliderInfo2D::ShapeVariant &to,
    float t) {
    if (from.index() != to.index()) {
        return to;
    }

    return std::visit(
        [t](const auto &a, const auto &b) -> ColliderInfo2D::ShapeVariant {
            using A = std::decay_t<decltype(a)>;
            using B = std::decay_t<decltype(b)>;

            if constexpr (!std::is_same_v<A, B>) {
                return b;
            } else if constexpr (std::is_same_v<A, Math::Point2D>) {
                Math::Point2D out = b;
                out.position = LerpVec2(a.position, b.position, t);
                return out;
            } else if constexpr (std::is_same_v<A, Math::Circle>) {
                Math::Circle out = b;
                out.center = LerpVec2(a.center, b.center, t);
                out.radius = LerpFloat(a.radius, b.radius, t);
                return out;
            } else if constexpr (std::is_same_v<A, Math::Rect>) {
                Math::Rect out = b;
                out.center = LerpVec2(a.center, b.center, t);
                out.halfSize = LerpVec2(a.halfSize, b.halfSize, t);
                return out;
            } else if constexpr (std::is_same_v<A, Math::Segment2D>) {
                Math::Segment2D out = b;
                out.start = LerpVec2(a.start, b.start, t);
                out.end = LerpVec2(a.end, b.end, t);
                return out;
            } else if constexpr (std::is_same_v<A, Math::Capsule2D>) {
                Math::Capsule2D out = b;
                out.start = LerpVec2(a.start, b.start, t);
                out.end = LerpVec2(a.end, b.end, t);
                out.radius = LerpFloat(a.radius, b.radius, t);
                return out;
            } else {
                return b;
            }
        },
        from, to);
}

inline ColliderInfo3D::ShapeVariant InterpolateShape3D(
    const ColliderInfo3D::ShapeVariant &from,
    const ColliderInfo3D::ShapeVariant &to,
    float t) {
    if (from.index() != to.index()) {
        return to;
    }

    return std::visit(
        [t](const auto &a, const auto &b) -> ColliderInfo3D::ShapeVariant {
            using A = std::decay_t<decltype(a)>;
            using B = std::decay_t<decltype(b)>;

            if constexpr (!std::is_same_v<A, B>) {
                return b;
            } else if constexpr (std::is_same_v<A, Math::Point3D>) {
                Math::Point3D out = b;
                out.position = LerpVec3(a.position, b.position, t);
                return out;
            } else if constexpr (std::is_same_v<A, Math::Sphere>) {
                Math::Sphere out = b;
                out.center = LerpVec3(a.center, b.center, t);
                out.radius = LerpFloat(a.radius, b.radius, t);
                return out;
            } else if constexpr (std::is_same_v<A, Math::AABB>) {
                Math::AABB out = b;
                out.min = LerpVec3(a.min, b.min, t);
                out.max = LerpVec3(a.max, b.max, t);
                return out;
            } else if constexpr (std::is_same_v<A, Math::OBB>) {
                Math::OBB out = b;
                out.center = LerpVec3(a.center, b.center, t);
                out.halfSize = LerpVec3(a.halfSize, b.halfSize, t);
                return out;
            } else if constexpr (std::is_same_v<A, Math::Plane>) {
                return b;
            } else {
                return b;
            }
        },
        from, to);
}

inline bool Intersects2DCCD(
    const ColliderInfo2D::ShapeVariant &currentA,
    const ColliderInfo2D::ShapeVariant *prevA,
    bool ccdA,
    const ColliderInfo2D::ShapeVariant &currentB,
    const ColliderInfo2D::ShapeVariant *prevB,
    bool ccdB) {
    if (!ccdA && !ccdB) {
        return Intersects2D(currentA, currentB);
    }

    const int samples = ComputeAdaptiveSamples2D(currentA, prevA, ccdA, currentB, prevB, ccdB);
    for (int i = 0; i <= samples; ++i) {
        const float t = static_cast<float>(i) / static_cast<float>(samples);

        const auto shapeA = (ccdA && prevA) ? InterpolateShape2D(*prevA, currentA, t) : currentA;
        const auto shapeB = (ccdB && prevB) ? InterpolateShape2D(*prevB, currentB, t) : currentB;

        if (Intersects2D(shapeA, shapeB)) {
            return true;
        }
    }

    return false;
}

inline bool Intersects3DCCD(
    const ColliderInfo3D::ShapeVariant &currentA,
    const ColliderInfo3D::ShapeVariant *prevA,
    bool ccdA,
    const ColliderInfo3D::ShapeVariant &currentB,
    const ColliderInfo3D::ShapeVariant *prevB,
    bool ccdB) {
    if (!ccdA && !ccdB) {
        return Intersects3D(currentA, currentB);
    }

    const int samples = ComputeAdaptiveSamples3D(currentA, prevA, ccdA, currentB, prevB, ccdB);
    for (int i = 0; i <= samples; ++i) {
        const float t = static_cast<float>(i) / static_cast<float>(samples);

        const auto shapeA = (ccdA && prevA) ? InterpolateShape3D(*prevA, currentA, t) : currentA;
        const auto shapeB = (ccdB && prevB) ? InterpolateShape3D(*prevB, currentB, t) : currentB;

        if (Intersects3D(shapeA, shapeB)) {
            return true;
        }
    }

    return false;
}

inline HitInfo2D ComputeHit2DCCD(
    const ColliderInfo2D::ShapeVariant &currentA,
    const ColliderInfo2D::ShapeVariant *prevA,
    bool ccdA,
    const ColliderInfo2D::ShapeVariant &currentB,
    const ColliderInfo2D::ShapeVariant *prevB,
    bool ccdB) {
    if (!ccdA && !ccdB) {
        return ComputeHit2D(currentA, currentB);
    }

    const int samples = ComputeAdaptiveSamples2D(currentA, prevA, ccdA, currentB, prevB, ccdB);
    for (int i = 0; i <= samples; ++i) {
        const float t = static_cast<float>(i) / static_cast<float>(samples);

        const auto shapeA = (ccdA && prevA) ? InterpolateShape2D(*prevA, currentA, t) : currentA;
        const auto shapeB = (ccdB && prevB) ? InterpolateShape2D(*prevB, currentB, t) : currentB;

        const HitInfo2D hi = ComputeHit2D(shapeA, shapeB);
        if (hi.isHit) {
            return hi;
        }
    }

    return ComputeHit2D(currentA, currentB);
}

inline HitInfo3D ComputeHit3DCCD(
    const ColliderInfo3D::ShapeVariant &currentA,
    const ColliderInfo3D::ShapeVariant *prevA,
    bool ccdA,
    const ColliderInfo3D::ShapeVariant &currentB,
    const ColliderInfo3D::ShapeVariant *prevB,
    bool ccdB) {
    if (!ccdA && !ccdB) {
        return ComputeHit3D(currentA, currentB);
    }

    const int samples = ComputeAdaptiveSamples3D(currentA, prevA, ccdA, currentB, prevB, ccdB);
    for (int i = 0; i <= samples; ++i) {
        const float t = static_cast<float>(i) / static_cast<float>(samples);

        const auto shapeA = (ccdA && prevA) ? InterpolateShape3D(*prevA, currentA, t) : currentA;
        const auto shapeB = (ccdB && prevB) ? InterpolateShape3D(*prevB, currentB, t) : currentB;

        const HitInfo3D hi = ComputeHit3D(shapeA, shapeB);
        if (hi.isHit) {
            return hi;
        }
    }

    return ComputeHit3D(currentA, currentB);
}

} // namespace

Collider::ColliderID Collider::Add(const ColliderInfo2D &info) {
    const ColliderID id = nextId_++;
    Entry<ColliderInfo2D> e;
    e.id = id;
    e.info = info;
    e.prevShape = info.shape;
    e.hasPrevShape = true;
    colliders2D_.push_back(std::move(e));
    return id;
}

Collider::ColliderID Collider::Add(const ColliderInfo3D &info) {
    const ColliderID id = nextId_++;
    Entry<ColliderInfo3D> e;
    e.id = id;
    e.info = info;
    e.prevShape = info.shape;
    e.hasPrevShape = true;
    colliders3D_.push_back(std::move(e));
    return id;
}

bool Collider::Remove2D(ColliderID id) {
    return EraseById(colliders2D_, id);
}

bool Collider::Remove3D(ColliderID id) {
    return EraseById(colliders3D_, id);
}

bool Collider::UpdateColliderInfo2D(ColliderID id, const ColliderInfo2D &info) {
    for (auto &e : colliders2D_) {
        if (e.id == id) {
            e.prevShape = e.info.shape;
            e.hasPrevShape = true;
            e.info = info;
            return true;
        }
    }
    return false;
}

bool Collider::UpdateColliderInfo3D(ColliderID id, const ColliderInfo3D &info) {
    for (auto &e : colliders3D_) {
        if (e.id == id) {
            e.prevShape = e.info.shape;
            e.hasPrevShape = true;
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
    colliders3D_.clear();
    prevPairs3D_.clear();
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

        const auto *prevA = ai.hasPrevShape ? &ai.prevShape : nullptr;
        const auto *prevB = bi.hasPrevShape ? &bi.prevShape : nullptr;
        if (Intersects2DCCD(ai.info.shape, prevA, ai.info.ccdEnabled, bi.info.shape, prevB, bi.info.ccdEnabled)) {
            hits.push_back({ai.id, bi.id});
        }
    }
    return hits;
}

std::vector<Collider::HitPair3D> Collider::CheckAll3D() const {
    std::vector<HitPair3D> hits;
    const auto &pairs = BuildCandidatePairs3D(colliders3D_);
    hits.reserve(pairs.size());

    for (const auto &pair : pairs) {
        const auto &ai = colliders3D_[pair.a];
        const auto &bi = colliders3D_[pair.b];

        if (!ai.info.enabled || !bi.info.enabled) continue;
        if (!ShouldTest(ai.info.attribute, ai.info.ignoreAttribute, bi.info.attribute) ||
            !ShouldTest(bi.info.attribute, bi.info.ignoreAttribute, ai.info.attribute)) {
            continue;
        }

        const auto *prevA = ai.hasPrevShape ? &ai.prevShape : nullptr;
        const auto *prevB = bi.hasPrevShape ? &bi.prevShape : nullptr;
        if (Intersects3DCCD(ai.info.shape, prevA, ai.info.ccdEnabled, bi.info.shape, prevB, bi.info.ccdEnabled)) {
            hits.push_back({ai.id, bi.id});
        }
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

    const auto *prevA = pa->hasPrevShape ? &pa->prevShape : nullptr;
    const auto *prevB = pb->hasPrevShape ? &pb->prevShape : nullptr;
    return Intersects2DCCD(pa->info.shape, prevA, pa->info.ccdEnabled, pb->info.shape, prevB, pb->info.ccdEnabled);
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

    const auto *prevA = pa->hasPrevShape ? &pa->prevShape : nullptr;
    const auto *prevB = pb->hasPrevShape ? &pb->prevShape : nullptr;
    return Intersects3DCCD(pa->info.shape, prevA, pa->info.ccdEnabled, pb->info.shape, prevB, pb->info.ccdEnabled);
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

    HitInfo2D hiB = hitInfo;
    hiB.selfObject = eb->info.ownerObject;
    hiB.otherObject = ea->info.ownerObject;

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

void Collider::Dispatch3D(ColliderID a, ColliderID b, const HitInfo3D &hitInfo, bool wasHit) {
    const auto *ea = Find3D(a);
    const auto *eb = Find3D(b);
    if (!ea || !eb) return;

    HitInfo3D hiA = hitInfo;
    hiA.selfObject = ea->info.ownerObject;
    hiA.otherObject = eb->info.ownerObject;

    HitInfo3D hiB = hitInfo;
    hiB.selfObject = eb->info.ownerObject;
    hiB.otherObject = ea->info.ownerObject;

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

void Collider::Update2D() {
    std::vector<std::uint64_t> cur;

    const auto pairs = BuildCandidatePairs2D(colliders2D_);
    cur.reserve(pairs.size());

    for (const auto &pair : pairs) {
        const auto &ai = colliders2D_[pair.a];
        const auto &bi = colliders2D_[pair.b];

        if (!ai.info.enabled || !bi.info.enabled) continue;
        if (!ShouldTest(ai.info.attribute, ai.info.ignoreAttribute, bi.info.attribute) ||
            !ShouldTest(bi.info.attribute, bi.info.ignoreAttribute, ai.info.attribute)) {
            continue;
        }

        const auto *prevA = ai.hasPrevShape ? &ai.prevShape : nullptr;
        const auto *prevB = bi.hasPrevShape ? &bi.prevShape : nullptr;
        const HitInfo2D hi = ComputeHit2DCCD(ai.info.shape, prevA, ai.info.ccdEnabled, bi.info.shape, prevB, bi.info.ccdEnabled);
        const std::uint64_t key = MakePairKey(ai.id, bi.id);

        const bool wasHit = std::binary_search(prevPairs2D_.begin(), prevPairs2D_.end(), key);
        Dispatch2D(ai.id, bi.id, hi, wasHit);

        if (hi.isHit) cur.push_back(key);
    }

    std::sort(cur.begin(), cur.end());
    prevPairs2D_ = std::move(cur);

    for (auto &e : colliders2D_) {
        e.prevShape = e.info.shape;
        e.hasPrevShape = true;
    }
}

void Collider::Update3D() {
    std::vector<std::uint64_t> cur;

    const auto &pairs = BuildCandidatePairs3D(colliders3D_);
    cur.reserve(pairs.size());

    for (const auto &pair : pairs) {
        const auto &ai = colliders3D_[pair.a];
        const auto &bi = colliders3D_[pair.b];

        if (!ai.info.enabled || !bi.info.enabled) continue;
        if (!ShouldTest(ai.info.attribute, ai.info.ignoreAttribute, bi.info.attribute) ||
            !ShouldTest(bi.info.attribute, bi.info.ignoreAttribute, ai.info.attribute)) {
            continue;
        }

        const auto *prevA = ai.hasPrevShape ? &ai.prevShape : nullptr;
        const auto *prevB = bi.hasPrevShape ? &bi.prevShape : nullptr;
        const HitInfo3D hi = ComputeHit3DCCD(ai.info.shape, prevA, ai.info.ccdEnabled, bi.info.shape, prevB, bi.info.ccdEnabled);
        const std::uint64_t key = MakePairKey(ai.id, bi.id);

        const bool wasHit = std::binary_search(prevPairs3D_.begin(), prevPairs3D_.end(), key);
        Dispatch3D(ai.id, bi.id, hi, wasHit);

        if (hi.isHit) cur.push_back(key);
    }

    std::sort(cur.begin(), cur.end());
    prevPairs3D_ = std::move(cur);

    for (auto &e : colliders3D_) {
        e.prevShape = e.info.shape;
        e.hasPrevShape = true;
    }
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

} // namespace KashipanEngine
