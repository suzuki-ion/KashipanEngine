#pragma once

#include "Math/Vector3.h"
#include "Objects/Collision/Collider.h"
#include "Utilities/MathUtils.h"

#include "Objects/MathObjects/3D/Point3D.h"
#include "Objects/MathObjects/3D/Sphere.h"
#include "Objects/MathObjects/3D/AABB.h"
#include "Objects/MathObjects/3D/Plane.h"
#include "Objects/MathObjects/3D/OBB.h"

#include <algorithm>
#include <cmath>
#include <limits>

namespace KashipanEngine {
namespace CollisionAlgorithms3D {

namespace {
inline float Clamp(float v, float mn, float mx) { return std::clamp(v, mn, mx); }

inline float DistanceSquared(const Vector3 &a, const Vector3 &b) {
    return MathUtils::LengthSquared(a - b);
}

inline Vector3 GetOBBAxisX(const Math::OBB &b) {
    // 列優先の基底（Matrix3x3 の乗算実装と整合）
    return Vector3{b.orientation.m[0][0], b.orientation.m[0][1], b.orientation.m[0][2]};
}
inline Vector3 GetOBBAxisY(const Math::OBB &b) {
    return Vector3{b.orientation.m[1][0], b.orientation.m[1][1], b.orientation.m[1][2]};
}
inline Vector3 GetOBBAxisZ(const Math::OBB &b) {
    return Vector3{b.orientation.m[2][0], b.orientation.m[2][1], b.orientation.m[2][2]};
}

inline float AbsDot(const Vector3 &a, const Vector3 &b) {
    return std::abs(MathUtils::Dot(a, b));
}

inline float ProjectRadius(const Math::OBB &b, const Vector3 &axis) {
    const Vector3 ax = GetOBBAxisX(b);
    const Vector3 ay = GetOBBAxisY(b);
    const Vector3 az = GetOBBAxisZ(b);
    return b.halfSize.x * AbsDot(ax, axis) + b.halfSize.y * AbsDot(ay, axis) + b.halfSize.z * AbsDot(az, axis);
}

inline bool OverlapOnAxis(const Math::OBB &a, const Math::OBB &b, const Vector3 &axis, const Vector3 &t) {
    const float len2 = MathUtils::LengthSquared(axis);
    if (len2 == 0.0f) return true; // 軸が平行（外積ゼロ） -> 判定をスキップ

    const float dist = std::abs(MathUtils::Dot(t, axis));
    const float ra = ProjectRadius(a, axis);
    const float rb = ProjectRadius(b, axis);
    return dist <= (ra + rb);
}

inline Vector3 ClosestPointOnOBB(const Math::OBB &b, const Vector3 &p) {
    Vector3 d = p - b.center;

    const Vector3 ax = GetOBBAxisX(b);
    const Vector3 ay = GetOBBAxisY(b);
    const Vector3 az = GetOBBAxisZ(b);

    float x = MathUtils::Dot(d, ax);
    float y = MathUtils::Dot(d, ay);
    float z = MathUtils::Dot(d, az);

    x = Clamp(x, -b.halfSize.x, b.halfSize.x);
    y = Clamp(y, -b.halfSize.y, b.halfSize.y);
    z = Clamp(z, -b.halfSize.z, b.halfSize.z);

    return b.center + ax * x + ay * y + az * z;
}

inline Vector3 ClosestPointOnAABB(const Math::AABB &b, const Vector3 &p) {
    return Vector3{
        Clamp(p.x, b.min.x, b.max.x),
        Clamp(p.y, b.min.y, b.max.y),
        Clamp(p.z, b.min.z, b.max.z)};
}

inline Vector3 NormalizeSafe(const Vector3 &v, const Vector3 &fallback) {
    const float len2 = MathUtils::LengthSquared(v);
    if (len2 == 0.0f) return fallback;
    const float invLen = 1.0f / std::sqrt(len2);
    return v * invLen;
}

inline HitInfo MakeNoHit() {
    HitInfo hi;
    hi.isHit = false;
    return hi;
}

inline HitInfo MakeHit(const Vector3 &normal, float penetration) {
    HitInfo hi;
    hi.isHit = true;
    hi.normal = normal;
    hi.penetration = std::max(0.0f, penetration);
    return hi;
}

inline Math::OBB ToOBB(const Math::AABB &b) {
    Math::OBB o;
    o.center = (b.min + b.max) * 0.5f;
    o.halfSize = (b.max - b.min) * 0.5f;
    o.orientation = Matrix3x3::Identity();
    return o;
}
} // namespace

// 既存の真偽値（bool）の衝突判定
inline bool Intersects(const Math::Point3D &a, const Math::Point3D &b) {
    return a.position == b.position;
}

inline bool Intersects(const Math::Sphere &s, const Math::Point3D &p) {
    return DistanceSquared(s.center, p.position) <= s.radius * s.radius;
}
inline bool Intersects(const Math::Point3D &p, const Math::Sphere &s) { return Intersects(s, p); }

inline bool Intersects(const Math::Sphere &a, const Math::Sphere &b) {
    const float r = a.radius + b.radius;
    return DistanceSquared(a.center, b.center) <= r * r;
}

inline bool Intersects(const Math::AABB &b, const Math::Point3D &p) {
    const Vector3 &v = p.position;
    return (b.min.x <= v.x && v.x <= b.max.x) && (b.min.y <= v.y && v.y <= b.max.y) && (b.min.z <= v.z && v.z <= b.max.z);
}
inline bool Intersects(const Math::Point3D &p, const Math::AABB &b) { return Intersects(b, p); }

inline bool Intersects(const Math::AABB &a, const Math::AABB &b) {
    return (a.min.x <= b.max.x && a.max.x >= b.min.x) &&
           (a.min.y <= b.max.y && a.max.y >= b.min.y) &&
           (a.min.z <= b.max.z && a.max.z >= b.min.z);
}

inline bool Intersects(const Math::Sphere &s, const Math::AABB &b) {
    const Vector3 closest = ClosestPointOnAABB(b, s.center);
    return DistanceSquared(s.center, closest) <= s.radius * s.radius;
}
inline bool Intersects(const Math::AABB &b, const Math::Sphere &s) { return Intersects(s, b); }

inline bool Intersects(const Math::Plane &pl, const Math::Point3D &p, float thickness = 0.0f) {
    const float d = MathUtils::Dot(pl.normal, p.position) + pl.distance;
    return std::abs(d) <= thickness;
}
inline bool Intersects(const Math::Point3D &p, const Math::Plane &pl, float thickness = 0.0f) { return Intersects(pl, p, thickness); }

inline bool Intersects(const Math::Plane &pl, const Math::Sphere &s) {
    const float d = MathUtils::Dot(pl.normal, s.center) + pl.distance;
    return std::abs(d) <= s.radius;
}
inline bool Intersects(const Math::Sphere &s, const Math::Plane &pl) { return Intersects(pl, s); }

inline bool Intersects(const Math::OBB &b, const Math::Point3D &p) {
    const Vector3 local = p.position - b.center;
    const Vector3 ax = GetOBBAxisX(b);
    const Vector3 ay = GetOBBAxisY(b);
    const Vector3 az = GetOBBAxisZ(b);

    const float x = MathUtils::Dot(local, ax);
    const float y = MathUtils::Dot(local, ay);
    const float z = MathUtils::Dot(local, az);

    return (std::abs(x) <= b.halfSize.x) && (std::abs(y) <= b.halfSize.y) && (std::abs(z) <= b.halfSize.z);
}
inline bool Intersects(const Math::Point3D &p, const Math::OBB &b) { return Intersects(b, p); }

inline bool Intersects(const Math::OBB &b, const Math::Sphere &s) {
    const Vector3 closest = ClosestPointOnOBB(b, s.center);
    return DistanceSquared(closest, s.center) <= s.radius * s.radius;
}
inline bool Intersects(const Math::Sphere &s, const Math::OBB &b) { return Intersects(b, s); }

inline bool Intersects(const Math::OBB &a, const Math::OBB &b) {
    const Vector3 aAxes[3] = {GetOBBAxisX(a), GetOBBAxisY(a), GetOBBAxisZ(a)};
    const Vector3 bAxes[3] = {GetOBBAxisX(b), GetOBBAxisY(b), GetOBBAxisZ(b)};
    const Vector3 t = b.center - a.center;

    // 15軸の SAT（分離軸定理）
    for (int i = 0; i < 3; ++i) {
        if (!OverlapOnAxis(a, b, aAxes[i], t)) return false;
        if (!OverlapOnAxis(a, b, bAxes[i], t)) return false;
    }

    for (int i = 0; i < 3; ++i) {
        for (int j = 0; j < 3; ++j) {
            const Vector3 axis = MathUtils::Cross(aAxes[i], bAxes[j]);
            if (!OverlapOnAxis(a, b, axis, t)) return false;
        }
    }

    return true;
}

//==================================================
// HitInfo の接触情報の計算
//==================================================

inline HitInfo ComputeHit(const Math::Sphere &a, const Math::Sphere &b) {
    const Vector3 d = b.center - a.center;
    const float dist2 = MathUtils::LengthSquared(d);
    const float r = a.radius + b.radius;
    if (dist2 > r * r) return MakeNoHit();

    const float dist = std::sqrt(std::max(0.0f, dist2));
    const Vector3 n = NormalizeSafe(d, Vector3{1.0f, 0.0f, 0.0f});
    const float penetration = r - dist;
    return MakeHit(n, penetration);
}

inline HitInfo ComputeHit(const Math::AABB &a, const Math::AABB &b) {
    const Vector3 ac = (a.min + a.max) * 0.5f;
    const Vector3 bc = (b.min + b.max) * 0.5f;
    const Vector3 ah = (a.max - a.min) * 0.5f;
    const Vector3 bh = (b.max - b.min) * 0.5f;

    const Vector3 d = bc - ac;
    const float px = (ah.x + bh.x) - std::abs(d.x);
    const float py = (ah.y + bh.y) - std::abs(d.y);
    const float pz = (ah.z + bh.z) - std::abs(d.z);

    if (px < 0.0f || py < 0.0f || pz < 0.0f) return MakeNoHit();

    if (px <= py && px <= pz) {
        const float sx = (d.x >= 0.0f) ? 1.0f : -1.0f;
        return MakeHit(Vector3{sx, 0.0f, 0.0f}, px);
    }
    if (py <= pz) {
        const float sy = (d.y >= 0.0f) ? 1.0f : -1.0f;
        return MakeHit(Vector3{0.0f, sy, 0.0f}, py);
    }

    const float sz = (d.z >= 0.0f) ? 1.0f : -1.0f;
    return MakeHit(Vector3{0.0f, 0.0f, sz}, pz);
}

inline HitInfo ComputeHit(const Math::Sphere &s, const Math::AABB &b) {
    const Vector3 closest = ClosestPointOnAABB(b, s.center);
    const Vector3 v = s.center - closest;
    const float dist2 = MathUtils::LengthSquared(v);
    if (dist2 > s.radius * s.radius) return MakeNoHit();

    const float dist = std::sqrt(std::max(0.0f, dist2));

    // AABB の内側にある場合 -> 面までの距離が最小になる軸を選ぶ
    const bool inside = (s.center.x >= b.min.x && s.center.x <= b.max.x) &&
                        (s.center.y >= b.min.y && s.center.y <= b.max.y) &&
                        (s.center.z >= b.min.z && s.center.z <= b.max.z);

    if (inside) {
        const float dxMin = s.center.x - b.min.x;
        const float dxMax = b.max.x - s.center.x;
        const float dyMin = s.center.y - b.min.y;
        const float dyMax = b.max.y - s.center.y;
        const float dzMin = s.center.z - b.min.z;
        const float dzMax = b.max.z - s.center.z;

        float minD = dxMin;
        Vector3 n{-1.0f, 0.0f, 0.0f};
        if (dxMax < minD) { minD = dxMax; n = Vector3{1.0f, 0.0f, 0.0f}; }
        if (dyMin < minD) { minD = dyMin; n = Vector3{0.0f, -1.0f, 0.0f}; }
        if (dyMax < minD) { minD = dyMax; n = Vector3{0.0f, 1.0f, 0.0f}; }
        if (dzMin < minD) { minD = dzMin; n = Vector3{0.0f, 0.0f, -1.0f}; }
        if (dzMax < minD) { minD = dzMax; n = Vector3{0.0f, 0.0f, 1.0f}; }

        return MakeHit(n, s.radius + minD);
    }

    const Vector3 n = NormalizeSafe(v, Vector3{1.0f, 0.0f, 0.0f});
    return MakeHit(n, s.radius - dist);
}

inline HitInfo ComputeHit(const Math::AABB &b, const Math::Sphere &s) {
    HitInfo hi = ComputeHit(s, b);
    if (hi.isHit) hi.normal = hi.normal * -1.0f;
    return hi;
}

inline HitInfo ComputeHit(const Math::Plane &pl, const Math::Sphere &s) {
    const float d = MathUtils::Dot(pl.normal, s.center) + pl.distance;
    const float ad = std::abs(d);
    if (ad > s.radius) return MakeNoHit();

    const Vector3 n = (d >= 0.0f) ? pl.normal : (pl.normal * -1.0f);
    return MakeHit(n, s.radius - ad);
}

inline HitInfo ComputeHit(const Math::Sphere &s, const Math::Plane &pl) {
    HitInfo hi = ComputeHit(pl, s);
    if (hi.isHit) hi.normal = hi.normal * -1.0f;
    return hi;
}

inline HitInfo ComputeHit(const Math::OBB &b, const Math::Sphere &s) {
    const Vector3 closest = ClosestPointOnOBB(b, s.center);
    const Vector3 v = s.center - closest;
    const float dist2 = MathUtils::LengthSquared(v);
    if (dist2 > s.radius * s.radius) return MakeNoHit();

    const float dist = std::sqrt(std::max(0.0f, dist2));
    const Vector3 n = NormalizeSafe(v, Vector3{1.0f, 0.0f, 0.0f});
    return MakeHit(n, s.radius - dist);
}

inline HitInfo ComputeHit(const Math::Sphere &s, const Math::OBB &b) {
    HitInfo hi = ComputeHit(b, s);
    if (hi.isHit) hi.normal = hi.normal * -1.0f;
    return hi;
}

inline HitInfo ComputeHit(const Math::Point3D &a, const Math::Point3D &b) {
    return Intersects(a, b) ? MakeHit(Vector3{1.0f, 0.0f, 0.0f}, 0.0f) : MakeNoHit();
}

inline HitInfo ComputeHit(const Math::Sphere &s, const Math::Point3D &p) {
    const Vector3 d = p.position - s.center;
    const float dist2 = MathUtils::LengthSquared(d);
    if (dist2 > s.radius * s.radius) return MakeNoHit();
    const float dist = std::sqrt(std::max(0.0f, dist2));
    const Vector3 n = NormalizeSafe(d, Vector3{1.0f, 0.0f, 0.0f});
    return MakeHit(n, s.radius - dist);
}
inline HitInfo ComputeHit(const Math::Point3D &p, const Math::Sphere &s) {
    HitInfo hi = ComputeHit(s, p);
    if (hi.isHit) hi.normal = hi.normal * -1.0f;
    return hi;
}

inline HitInfo ComputeHit(const Math::AABB &b, const Math::Point3D &p) {
    if (!Intersects(b, p)) return MakeNoHit();
    const Vector3 c = (b.min + b.max) * 0.5f;
    const Vector3 h = (b.max - b.min) * 0.5f;
    const Vector3 d = p.position - c;

    const float px = h.x - std::abs(d.x);
    const float py = h.y - std::abs(d.y);
    const float pz = h.z - std::abs(d.z);

    if (px <= py && px <= pz) {
        const float sx = (d.x >= 0.0f) ? 1.0f : -1.0f;
        return MakeHit(Vector3{sx, 0.0f, 0.0f}, px);
    }
    if (py <= pz) {
        const float sy = (d.y >= 0.0f) ? 1.0f : -1.0f;
        return MakeHit(Vector3{0.0f, sy, 0.0f}, py);
    }
    const float sz = (d.z >= 0.0f) ? 1.0f : -1.0f;
    return MakeHit(Vector3{0.0f, 0.0f, sz}, pz);
}
inline HitInfo ComputeHit(const Math::Point3D &p, const Math::AABB &b) {
    HitInfo hi = ComputeHit(b, p);
    if (hi.isHit) hi.normal = hi.normal * -1.0f;
    return hi;
}

namespace {
inline bool GetOverlapOnAxisSAT(const Math::OBB &a, const Math::OBB &b, const Vector3 &axis, const Vector3 &t,
    float &outPenetration) {
    const float len2 = MathUtils::LengthSquared(axis);
    if (len2 == 0.0f) {
        outPenetration = std::numeric_limits<float>::infinity();
        return true;
    }

    const float invLen = 1.0f / std::sqrt(len2);
    const Vector3 nAxis = axis * invLen;

    const float dist = std::abs(MathUtils::Dot(t, nAxis));
    const float ra = ProjectRadius(a, nAxis);
    const float rb = ProjectRadius(b, nAxis);
    const float overlap = (ra + rb) - dist;

    if (overlap < 0.0f) {
        outPenetration = overlap;
        return false;
    }

    outPenetration = overlap;
    return true;
}

} // namespace

inline HitInfo ComputeHit(const Math::OBB &a, const Math::OBB &b) {
    // SAT（分離軸定理）で、貫通量が最小の軸を接触法線として採用する
    const Vector3 aAxes[3] = {GetOBBAxisX(a), GetOBBAxisY(a), GetOBBAxisZ(a)};
    const Vector3 bAxes[3] = {GetOBBAxisX(b), GetOBBAxisY(b), GetOBBAxisZ(b)};
    const Vector3 t = b.center - a.center;

    float bestPen = std::numeric_limits<float>::infinity();
    Vector3 bestAxis{1.0f, 0.0f, 0.0f};

    auto consider = [&](const Vector3 &axis) {
        float pen = 0.0f;
        if (!GetOverlapOnAxisSAT(a, b, axis, t, pen)) {
            bestPen = pen;
            return false;
        }
        if (pen < bestPen) {
            bestPen = pen;
            bestAxis = axis;
        }
        return true;
    };

    // 15軸
    for (int i = 0; i < 3; ++i) {
        if (!consider(aAxes[i])) return MakeNoHit();
        if (!consider(bAxes[i])) return MakeNoHit();
    }
    for (int i = 0; i < 3; ++i) {
        for (int j = 0; j < 3; ++j) {
            const Vector3 axis = MathUtils::Cross(aAxes[i], bAxes[j]);
            if (!consider(axis)) return MakeNoHit();
        }
    }

    // a -> b 方向に合わせて法線の向きを決定する
    Vector3 n = NormalizeSafe(bestAxis, Vector3{1.0f, 0.0f, 0.0f});
    if (MathUtils::Dot(n, t) < 0.0f) n = n * -1.0f;

    return MakeHit(n, bestPen);
}

} // namespace CollisionAlgorithms3D
} // namespace KashipanEngine
