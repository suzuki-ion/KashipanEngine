#pragma once

#include <bitset>
#include <cstddef>
#include <cstdint>
#include <functional>
#include <variant>
#include <vector>

#include "Math/Vector3.h"

#include "Objects/MathObjects/2D/Capsule2D.h"
#include "Objects/MathObjects/2D/Circle.h"
#include "Objects/MathObjects/2D/Point2D.h"
#include "Objects/MathObjects/2D/Rect.h"
#include "Objects/MathObjects/2D/Segment.h"

#include "Objects/MathObjects/3D/AABB.h"
#include "Objects/MathObjects/3D/OBB.h"
#include "Objects/MathObjects/3D/Plane.h"
#include "Objects/MathObjects/3D/Point3D.h"
#include "Objects/MathObjects/3D/Sphere.h"

namespace KashipanEngine {

struct HitInfo final {
    bool isHit = false;
    Vector3 normal{0.0f, 0.0f, 0.0f};
    float penetration = 0.0f;
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

    std::bitset<kMaxAttributes> attribute{};
    std::bitset<kMaxAttributes> ignoreAttribute{};

    std::function<void(const HitInfo &hitInfo)> onCollisionEnter;
    std::function<void(const HitInfo &hitInfo)> onCollisionStay;
    std::function<void(const HitInfo &hitInfo)> onCollisionExit;

    bool enabled = true;
};

struct ColliderInfo3D final {
    static constexpr std::size_t kMaxAttributes = 32;

    using ShapeVariant = std::variant<
        Math::Point3D,
        Math::Sphere,
        Math::AABB,
        Math::OBB,
        Math::Plane>;

    ShapeVariant shape{};

    std::bitset<kMaxAttributes> attribute{};
    std::bitset<kMaxAttributes> ignoreAttribute{};

    std::function<void(const HitInfo &hitInfo)> onCollisionEnter;
    std::function<void(const HitInfo &hitInfo)> onCollisionStay;
    std::function<void(const HitInfo &hitInfo)> onCollisionExit;

    bool enabled = true;
};

class Collider final {
public:
    using ColliderID = std::uint32_t;

    struct HitPair2D {
        ColliderID a = 0;
        ColliderID b = 0;
    };

    struct HitPair3D {
        ColliderID a = 0;
        ColliderID b = 0;
    };

    Collider() = default;

    ColliderID Add(const ColliderInfo2D &info);
    ColliderID Add(const ColliderInfo3D &info);

    bool Remove2D(ColliderID id);
    bool Remove3D(ColliderID id);

    void Clear2D();
    void Clear3D();

    std::vector<HitPair2D> CheckAll2D() const;
    std::vector<HitPair3D> CheckAll3D() const;

    bool Check2D(ColliderID a, ColliderID b) const;
    bool Check3D(ColliderID a, ColliderID b) const;

    void Update2D();
    void Update3D();

private:
    template<typename Info>
    struct Entry {
        ColliderID id;
        Info info;
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

    static std::uint64_t MakePairKey(ColliderID a, ColliderID b);

    void Dispatch2D(ColliderID a, ColliderID b, const HitInfo &hitInfo, bool wasHit);
    void Dispatch3D(ColliderID a, ColliderID b, const HitInfo &hitInfo, bool wasHit);

    std::vector<std::uint64_t> prevPairs2D_;
    std::vector<std::uint64_t> prevPairs3D_;

    ColliderID nextId_ = 1;
    std::vector<Entry<ColliderInfo2D>> colliders2D_;
    std::vector<Entry<ColliderInfo3D>> colliders3D_;
};

} // namespace KashipanEngine
