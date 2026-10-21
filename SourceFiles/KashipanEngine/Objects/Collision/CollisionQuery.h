#pragma once

#include <cmath>
#include "Objects/Collision/Collider.h"
#include "Objects/Collision/CollisionAlgorithms2D.h"
#include "Math/Quaternion.h"

namespace KashipanEngine {

enum class CollisionShapeType2D { Point, Circle, Box, Segment, Capsule };
enum class CollisionShapeType3D { Sphere, Box, Capsule, ConvexMesh };

// Query shapes use world coordinates; box dimensions are half extents.
struct CollisionShape2D {
    CollisionShapeType2D type = CollisionShapeType2D::Circle;
    Vector2 center{};
    Vector2 halfSize{0.5f, 0.5f};
    Vector2 start{};
    Vector2 end{0.0f, 1.0f};
    float radius = 0.5f;
    float rotation = 0.0f;

    ColliderInfo2D::ShapeVariant Build() const {
        switch (type) {
        case CollisionShapeType2D::Point: return Math::Point2D{center};
        case CollisionShapeType2D::Circle: return Math::Circle{center, radius};
        case CollisionShapeType2D::Box: return Math::Rect{center, halfSize, rotation};
        case CollisionShapeType2D::Segment: return Math::Segment2D{start, end};
        case CollisionShapeType2D::Capsule: return Math::Capsule2D{start, end, radius};
        }
        return Math::Point2D{center};
    }
};

struct CollisionShape3D {
    CollisionShapeType3D type = CollisionShapeType3D::Sphere;
    Vector3 center{};
    Vector3 halfExtents{0.5f, 0.5f, 0.5f};
    Quaternion rotation = Quaternion::Identity();
    float radius = 0.5f;
    // Length of the capsule's central segment, excluding its hemispheres.
    float height = 1.0f;
    std::vector<Vector3> vertices;

    ColliderInfo3D Build() const {
        ColliderInfo3D info;
        switch (type) {
        case CollisionShapeType3D::Sphere: info.shape = ColliderInfo3D::SphereShape3D{center, radius}; break;
        case CollisionShapeType3D::Box: info.shape = ColliderInfo3D::BoxShape3D{center, halfExtents}; break;
        case CollisionShapeType3D::Capsule: info.shape = ColliderInfo3D::CapsuleShape3D{center, radius, height}; break;
        case CollisionShapeType3D::ConvexMesh:
            info.shape = ColliderInfo3D::ConvexMeshShape3D{vertices}; break;
        }
        return info;
    }
};

inline bool IsFinite(const Vector2 &v) { return std::isfinite(v.x) && std::isfinite(v.y); }
inline bool IsFinite(const Vector3 &v) { return std::isfinite(v.x) && std::isfinite(v.y) && std::isfinite(v.z); }

inline bool IsValidQueryShape(const ColliderInfo2D::ShapeVariant &shape) {
    return std::visit([](const auto &s) {
        using T = std::decay_t<decltype(s)>;
        if constexpr (std::is_same_v<T, Math::Point2D>) return IsFinite(s.position);
        else if constexpr (std::is_same_v<T, Math::Rect>)
            return IsFinite(s.center) && IsFinite(s.halfSize) && s.halfSize.x >= 0 && s.halfSize.y >= 0 && std::isfinite(s.rotation);
        else if constexpr (std::is_same_v<T, Math::Circle>)
            return IsFinite(s.center) && std::isfinite(s.radius) && s.radius >= 0;
        else if constexpr (std::is_same_v<T, Math::Segment2D>) return IsFinite(s.start) && IsFinite(s.end);
        else return IsFinite(s.start) && IsFinite(s.end) && std::isfinite(s.radius) && s.radius >= 0;
    }, shape);
}

inline HitInfo ComputeQueryHit2D(const ColliderInfo2D::ShapeVariant &a, const ColliderInfo2D::ShapeVariant &b) {
    // A zero-radius capsule also handles segment-box and segment-capsule pairs.
    using Shape = std::variant<Math::Point2D, Math::Circle, Math::Rect, Math::Capsule2D>;
    const auto convert = [](const ColliderInfo2D::ShapeVariant &shape) -> Shape {
        return std::visit([](const auto &s) -> Shape {
            if constexpr (std::is_same_v<std::decay_t<decltype(s)>, Math::Segment2D>)
                return Math::Capsule2D{s.start, s.end, 0.0f};
            else return s;
        }, shape);
    };
    return std::visit([](const auto &x, const auto &y) { return CollisionAlgorithms2D::ComputeHit(x, y); }, convert(a), convert(b));
}

} // namespace KashipanEngine
