#pragma once

#include "Math/Vector2.h"
#include "Math/Vector3.h"
#include "Objects/Collision/Collider.h"
#include "Utilities/MathUtils.h"

#include "Objects/MathObjects/2D/Point2D.h"
#include "Objects/MathObjects/2D/Circle.h"
#include "Objects/MathObjects/2D/Rect.h"
#include "Objects/MathObjects/2D/Segment.h"
#include "Objects/MathObjects/2D/Capsule2D.h"

#include <algorithm>
#include <cmath>
#include <limits>

namespace KashipanEngine {
namespace CollisionAlgorithms2D {

namespace {
inline float Clamp01(float v) { return std::clamp(v, 0.0f, 1.0f); }

inline float Clamp(float v, float mn, float mx) { return std::clamp(v, mn, mx); }

inline Vector2 ClosestPointOnSegment(const Math::Segment2D &s, const Vector2 &p) {
    const Vector2 ab = s.end - s.start;
    const float denom = MathUtils::Dot(ab, ab);
    if (denom == 0.0f) return s.start;
    const float t = Clamp01(MathUtils::Dot(p - s.start, ab) / denom);
    return s.start + ab * t;
}

inline Vector2 ClosestPointOnRectLocal(const Vector2 &halfSize, const Vector2 &localP) {
    return Vector2{Clamp(localP.x, -halfSize.x, halfSize.x), Clamp(localP.y, -halfSize.y, halfSize.y)};
}

/// @brief ベクトルをcos/sinで回転させる
inline Vector2 RotateVector2(const Vector2 &v, float cosA, float sinA) {
    return Vector2(v.x * cosA - v.y * sinA, v.x * sinA + v.y * cosA);
}

/// @brief ワールド座標の点を矩形のローカル空間（矩形中心が原点、回転無し）に変換する
inline Vector2 WorldToRectLocal(const Math::Rect &r, const Vector2 &worldPoint) {
    const Vector2 d = worldPoint - r.center;
    if (r.rotation == 0.0f) return d;
    return RotateVector2(d, std::cos(-r.rotation), std::sin(-r.rotation));
}

/// @brief 矩形のローカル空間の向き（法線等、平行移動を伴わない量）をワールド空間へ変換する
inline Vector2 RectLocalDirectionToWorld(const Math::Rect &r, const Vector2 &localDir) {
    if (r.rotation == 0.0f) return localDir;
    return RotateVector2(localDir, std::cos(r.rotation), std::sin(r.rotation));
}

/// @brief 矩形（OBB）の2つの軸（ワールド方向）と4頂点（ワールド座標）を求める
inline void GetRectAxesAndCorners(const Math::Rect &r, Vector2 axes[2], Vector2 corners[4]) {
    const float c = std::cos(r.rotation);
    const float s = std::sin(r.rotation);
    axes[0] = Vector2(c, s);
    axes[1] = Vector2(-s, c);
    const Vector2 ex = axes[0] * r.halfSize.x;
    const Vector2 ey = axes[1] * r.halfSize.y;
    corners[0] = r.center - ex - ey;
    corners[1] = r.center + ex - ey;
    corners[2] = r.center + ex + ey;
    corners[3] = r.center - ex + ey;
}

inline void ProjectOntoAxis(const Vector2 corners[4], const Vector2 &axis, float &outMin, float &outMax) {
    float minP = MathUtils::Dot(corners[0], axis);
    float maxP = minP;
    for (int i = 1; i < 4; ++i) {
        const float p = MathUtils::Dot(corners[i], axis);
        minP = std::min(minP, p);
        maxP = std::max(maxP, p);
    }
    outMin = minP;
    outMax = maxP;
}

inline float Clamp01f(float v) { return std::clamp(v, 0.0f, 1.0f); }

inline Vector2 ClosestPointBetweenSegments(const Vector2 &p1, const Vector2 &q1, const Vector2 &p2, const Vector2 &q2,
    float &outS, float &outT) {
    // 「Real-Time Collision Detection」（Christer Ericson）に基づく、線分同士の最近接点計算
    const Vector2 d1 = q1 - p1;
    const Vector2 d2 = q2 - p2;
    const Vector2 r = p1 - p2;
    const float a = MathUtils::Dot(d1, d1);
    const float e = MathUtils::Dot(d2, d2);
    const float f = MathUtils::Dot(d2, r);

    float s = 0.0f;
    float t = 0.0f;

    if (a <= 0.0f && e <= 0.0f) {
        outS = outT = 0.0f;
        return p1; // 両方とも退化（長さ0）
    }

    if (a <= 0.0f) {
        s = 0.0f;
        t = Clamp01f(f / e);
    } else {
        const float c = MathUtils::Dot(d1, r);
        if (e <= 0.0f) {
            t = 0.0f;
            s = Clamp01f(-c / a);
        } else {
            const float b = MathUtils::Dot(d1, d2);
            const float denom = a * e - b * b;

            if (denom != 0.0f) {
                s = Clamp01f((b * f - c * e) / denom);
            } else {
                s = 0.0f;
            }

            t = (b * s + f) / e;

            if (t < 0.0f) {
                t = 0.0f;
                s = Clamp01f(-c / a);
            } else if (t > 1.0f) {
                t = 1.0f;
                s = Clamp01f((b - c) / a);
            }
        }
    }

    outS = s;
    outT = t;
    const Vector2 c1 = p1 + d1 * s;
    // 線分1上の最近接点を返す（呼び出し側で同様に c2 を計算可能）
    return c1;
}

inline Vector2 PointOnSegment(const Vector2 &a, const Vector2 &b, float t) {
    return a + (b - a) * t;
}

inline float DistanceSquared(const Vector2 &a, const Vector2 &b) {
    return MathUtils::LengthSquared(a - b);
}

inline Vector3 ToNormal3(const Vector2 &n) {
    return Vector3{n.x, n.y, 0.0f};
}

inline Vector2 NormalizeSafe(const Vector2 &v, const Vector2 &fallback) {
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

inline HitInfo MakeHit(const Vector2 &normal, float penetration) {
    HitInfo hi;
    hi.isHit = true;
    hi.normal = ToNormal3(normal);
    hi.penetration = std::max(0.0f, penetration);
    return hi;
}
} // namespace

// 既存の真偽値（bool）の衝突判定
inline bool Intersects(const Math::Point2D &a, const Math::Point2D &b) {
    return a.position == b.position;
}

inline bool Intersects(const Math::Circle &c, const Math::Point2D &p) {
    return DistanceSquared(c.center, p.position) <= c.radius * c.radius;
}
inline bool Intersects(const Math::Point2D &p, const Math::Circle &c) { return Intersects(c, p); }

inline bool Intersects(const Math::Circle &a, const Math::Circle &b) {
    const float r = a.radius + b.radius;
    return DistanceSquared(a.center, b.center) <= r * r;
}

inline bool Intersects(const Math::Rect &r, const Math::Point2D &p) {
    const Vector2 local = WorldToRectLocal(r, p.position);
    return std::abs(local.x) <= r.halfSize.x && std::abs(local.y) <= r.halfSize.y;
}
inline bool Intersects(const Math::Point2D &p, const Math::Rect &r) { return Intersects(r, p); }

inline bool Intersects(const Math::Rect &a, const Math::Rect &b) {
    Vector2 axesA[2], cornersA[4];
    Vector2 axesB[2], cornersB[4];
    GetRectAxesAndCorners(a, axesA, cornersA);
    GetRectAxesAndCorners(b, axesB, cornersB);
    const Vector2 axes[4] = {axesA[0], axesA[1], axesB[0], axesB[1]};
    for (const auto &axis : axes) {
        float minA, maxA, minB, maxB;
        ProjectOntoAxis(cornersA, axis, minA, maxA);
        ProjectOntoAxis(cornersB, axis, minB, maxB);
        if (maxA < minB || maxB < minA) return false;
    }
    return true;
}

inline bool Intersects(const Math::Circle &c, const Math::Rect &r) {
    const Vector2 localCenter = WorldToRectLocal(r, c.center);
    const Vector2 closestLocal = ClosestPointOnRectLocal(r.halfSize, localCenter);
    return DistanceSquared(localCenter, closestLocal) <= c.radius * c.radius;
}
inline bool Intersects(const Math::Rect &r, const Math::Circle &c) { return Intersects(c, r); }

inline bool Intersects(const Math::Segment2D &s, const Math::Point2D &p, float thickness = 0.0f) {
    const Vector2 cp = ClosestPointOnSegment(s, p.position);
    return DistanceSquared(cp, p.position) <= thickness * thickness;
}

inline bool Intersects(const Math::Segment2D &s, const Math::Circle &c) {
    const Vector2 cp = ClosestPointOnSegment(s, c.center);
    return DistanceSquared(cp, c.center) <= c.radius * c.radius;
}
inline bool Intersects(const Math::Circle &c, const Math::Segment2D &s) { return Intersects(s, c); }

inline bool Intersects(const Math::Capsule2D &cap, const Math::Point2D &p) {
    const Math::Segment2D seg{cap.start, cap.end};
    const Vector2 cp = ClosestPointOnSegment(seg, p.position);
    return DistanceSquared(cp, p.position) <= cap.radius * cap.radius;
}
inline bool Intersects(const Math::Point2D &p, const Math::Capsule2D &cap) { return Intersects(cap, p); }

inline bool Intersects(const Math::Capsule2D &cap, const Math::Circle &c) {
    const Math::Segment2D seg{cap.start, cap.end};
    const Vector2 cp = ClosestPointOnSegment(seg, c.center);
    const float r = cap.radius + c.radius;
    return DistanceSquared(cp, c.center) <= r * r;
}
inline bool Intersects(const Math::Circle &c, const Math::Capsule2D &cap) { return Intersects(cap, c); }

inline bool Intersects(const Math::Capsule2D &a, const Math::Capsule2D &b) {
    const Math::Segment2D sa{a.start, a.end};
    const Vector2 cp = ClosestPointOnSegment(sa, b.start);
    const float r = a.radius + b.radius;
    const float d2 = DistanceSquared(cp, b.start);
    return d2 <= r * r;
}

//==================================================
// HitInfo contact computation
//==================================================

inline HitInfo ComputeHit(const Math::Circle &a, const Math::Circle &b) {
    const Vector2 d = b.center - a.center;
    const float dist2 = MathUtils::LengthSquared(d);
    const float r = a.radius + b.radius;
    if (dist2 > r * r) return MakeNoHit();

    const float dist = std::sqrt(std::max(0.0f, dist2));
    const Vector2 n = NormalizeSafe(d, Vector2{1.0f, 0.0f});
    const float penetration = r - dist;
    return MakeHit(n, penetration);
}

inline HitInfo ComputeHit(const Math::Rect &a, const Math::Rect &b) {
    Vector2 axesA[2], cornersA[4];
    Vector2 axesB[2], cornersB[4];
    GetRectAxesAndCorners(a, axesA, cornersA);
    GetRectAxesAndCorners(b, axesB, cornersB);
    const Vector2 axes[4] = {axesA[0], axesA[1], axesB[0], axesB[1]};

    float bestOverlap = std::numeric_limits<float>::max();
    Vector2 bestAxis{1.0f, 0.0f};

    for (const auto &axis : axes) {
        float minA, maxA, minB, maxB;
        ProjectOntoAxis(cornersA, axis, minA, maxA);
        ProjectOntoAxis(cornersB, axis, minB, maxB);
        const float overlap = std::min(maxA, maxB) - std::max(minA, minB);
        if (overlap < 0.0f) return MakeNoHit();
        if (overlap < bestOverlap) {
            bestOverlap = overlap;
            bestAxis = axis;
        }
    }

    const Vector2 d = b.center - a.center;
    if (MathUtils::Dot(d, bestAxis) < 0.0f) bestAxis = bestAxis * -1.0f;
    return MakeHit(bestAxis, bestOverlap);
}

inline HitInfo ComputeHit(const Math::Circle &c, const Math::Rect &r) {
    const Vector2 localCenter = WorldToRectLocal(r, c.center);
    const Vector2 closestLocal = ClosestPointOnRectLocal(r.halfSize, localCenter);

    const Vector2 vLocal = localCenter - closestLocal;
    const float dist2 = MathUtils::LengthSquared(vLocal);
    const float rr = c.radius * c.radius;
    if (dist2 > rr) return MakeNoHit();

    const float dist = std::sqrt(std::max(0.0f, dist2));

    // 中心が矩形内部にある場合は最小軸方向への押し出しを選ぶ
    const bool inside = (localCenter.x >= -r.halfSize.x && localCenter.x <= r.halfSize.x) &&
                        (localCenter.y >= -r.halfSize.y && localCenter.y <= r.halfSize.y);

    if (inside) {
        const float left = localCenter.x - (-r.halfSize.x);
        const float right = r.halfSize.x - localCenter.x;
        const float down = localCenter.y - (-r.halfSize.y);
        const float up = r.halfSize.y - localCenter.y;

        float minAxis = left;
        Vector2 nLocal{-1.0f, 0.0f};
        if (right < minAxis) { minAxis = right; nLocal = Vector2{1.0f, 0.0f}; }
        if (down < minAxis) { minAxis = down; nLocal = Vector2{0.0f, -1.0f}; }
        if (up < minAxis) { minAxis = up; nLocal = Vector2{0.0f, 1.0f}; }

        return MakeHit(RectLocalDirectionToWorld(r, nLocal), c.radius + minAxis);
    }

    const Vector2 nLocal = NormalizeSafe(vLocal, Vector2{1.0f, 0.0f});
    const float penetration = c.radius - dist;
    return MakeHit(RectLocalDirectionToWorld(r, nLocal), penetration);
}

inline HitInfo ComputeHit(const Math::Rect &r, const Math::Circle &c) {
    // Invert normal for opposite ordering
    HitInfo hi = ComputeHit(c, r);
    if (hi.isHit) {
        hi.normal = hi.normal * -1.0f;
    }
    return hi;
}

inline HitInfo ComputeHit(const Math::Point2D &a, const Math::Point2D &b) {
    return Intersects(a, b) ? MakeHit(Vector2{1.0f, 0.0f}, 0.0f) : MakeNoHit();
}

inline HitInfo ComputeHit(const Math::Circle &c, const Math::Point2D &p) {
    const Vector2 d = p.position - c.center;
    const float dist2 = MathUtils::LengthSquared(d);
    if (dist2 > c.radius * c.radius) return MakeNoHit();
    const float dist = std::sqrt(std::max(0.0f, dist2));
    const Vector2 n = NormalizeSafe(d, Vector2{1.0f, 0.0f});
    return MakeHit(n, c.radius - dist);
}
inline HitInfo ComputeHit(const Math::Point2D &p, const Math::Circle &c) {
    HitInfo hi = ComputeHit(c, p);
    if (hi.isHit) hi.normal = hi.normal * -1.0f;
    return hi;
}

inline HitInfo ComputeHit(const Math::Rect &r, const Math::Point2D &p) {
    const Vector2 local = WorldToRectLocal(r, p.position);
    if (std::abs(local.x) > r.halfSize.x || std::abs(local.y) > r.halfSize.y) return MakeNoHit();
    const float px = r.halfSize.x - std::abs(local.x);
    const float py = r.halfSize.y - std::abs(local.y);
    if (px < py) {
        const float sx = (local.x >= 0.0f) ? 1.0f : -1.0f;
        return MakeHit(RectLocalDirectionToWorld(r, Vector2{sx, 0.0f}), px);
    }
    const float sy = (local.y >= 0.0f) ? 1.0f : -1.0f;
    return MakeHit(RectLocalDirectionToWorld(r, Vector2{0.0f, sy}), py);
}
inline HitInfo ComputeHit(const Math::Point2D &p, const Math::Rect &r) {
    HitInfo hi = ComputeHit(r, p);
    if (hi.isHit) hi.normal = hi.normal * -1.0f;
    return hi;
}

inline HitInfo ComputeHit(const Math::Segment2D &a, const Math::Point2D &p, float thickness = 0.0f) {
    const Vector2 cp = ClosestPointOnSegment(a, p.position);
    const Vector2 d = p.position - cp;
    const float dist2 = MathUtils::LengthSquared(d);
    if (dist2 > thickness * thickness) return MakeNoHit();
    const float dist = std::sqrt(std::max(0.0f, dist2));
    const Vector2 n = NormalizeSafe(d, Vector2{1.0f, 0.0f});
    return MakeHit(n, thickness - dist);
}

inline HitInfo ComputeHit(const Math::Point2D &p, const Math::Segment2D &s, float thickness = 0.0f) {
    HitInfo hi = ComputeHit(s, p, thickness);
    if (hi.isHit) hi.normal = hi.normal * -1.0f;
    return hi;
}

inline HitInfo ComputeHit(const Math::Segment2D &s, const Math::Circle &c) {
    const Vector2 cp = ClosestPointOnSegment(s, c.center);
    const Vector2 d = c.center - cp;
    const float dist2 = MathUtils::LengthSquared(d);
    if (dist2 > c.radius * c.radius) return MakeNoHit();
    const float dist = std::sqrt(std::max(0.0f, dist2));
    const Vector2 n = NormalizeSafe(d, Vector2{1.0f, 0.0f});
    return MakeHit(n, c.radius - dist);
}

inline HitInfo ComputeHit(const Math::Circle &c, const Math::Segment2D &s) {
    HitInfo hi = ComputeHit(s, c);
    if (hi.isHit) hi.normal = hi.normal * -1.0f;
    return hi;
}

inline HitInfo ComputeHit(const Math::Segment2D &a, const Math::Segment2D &b, float thickness = 0.0f) {
    float s = 0.0f;
    float t = 0.0f;
    (void)ClosestPointBetweenSegments(a.start, a.end, b.start, b.end, s, t);
    const Vector2 ca = PointOnSegment(a.start, a.end, s);
    const Vector2 cb = PointOnSegment(b.start, b.end, t);
    const Vector2 d = cb - ca;
    const float dist2 = MathUtils::LengthSquared(d);
    if (dist2 > thickness * thickness) return MakeNoHit();
    const float dist = std::sqrt(std::max(0.0f, dist2));
    const Vector2 n = NormalizeSafe(d, Vector2{1.0f, 0.0f});
    return MakeHit(n, thickness - dist);
}

inline HitInfo ComputeHit(const Math::Capsule2D &cap, const Math::Point2D &p) {
    const Math::Segment2D seg{cap.start, cap.end};
    const Vector2 cp = ClosestPointOnSegment(seg, p.position);
    const Vector2 d = p.position - cp;
    const float dist2 = MathUtils::LengthSquared(d);
    if (dist2 > cap.radius * cap.radius) return MakeNoHit();
    const float dist = std::sqrt(std::max(0.0f, dist2));
    const Vector2 n = NormalizeSafe(d, Vector2{1.0f, 0.0f});
    return MakeHit(n, cap.radius - dist);
}

inline HitInfo ComputeHit(const Math::Point2D &p, const Math::Capsule2D &cap) {
    HitInfo hi = ComputeHit(cap, p);
    if (hi.isHit) hi.normal = hi.normal * -1.0f;
    return hi;
}

inline HitInfo ComputeHit(const Math::Capsule2D &cap, const Math::Circle &c) {
    const Math::Segment2D seg{cap.start, cap.end};
    const Vector2 cp = ClosestPointOnSegment(seg, c.center);
    const Vector2 d = c.center - cp;
    const float r = cap.radius + c.radius;
    const float dist2 = MathUtils::LengthSquared(d);
    if (dist2 > r * r) return MakeNoHit();
    const float dist = std::sqrt(std::max(0.0f, dist2));
    const Vector2 n = NormalizeSafe(d, Vector2{1.0f, 0.0f});
    return MakeHit(n, r - dist);
}

inline HitInfo ComputeHit(const Math::Circle &c, const Math::Capsule2D &cap) {
    HitInfo hi = ComputeHit(cap, c);
    if (hi.isHit) hi.normal = hi.normal * -1.0f;
    return hi;
}

inline HitInfo ComputeHit(const Math::Capsule2D &cap, const Math::Rect &r) {
    // 矩形のローカル空間（矩形中心が原点、回転無し）に持ち込んで、軸並行の矩形として判定する
    const Vector2 localStart = WorldToRectLocal(r, cap.start);
    const Vector2 localEnd = WorldToRectLocal(r, cap.end);
    const Math::Segment2D localSeg{localStart, localEnd};

    // 各端点および線分自体の最近接点を使って近似的に判定する
    Vector2 bestP = localStart;
    Vector2 bestQ = ClosestPointOnRectLocal(r.halfSize, localStart);
    float bestD2 = MathUtils::LengthSquared(bestP - bestQ);

    {
        const Vector2 q = ClosestPointOnRectLocal(r.halfSize, localEnd);
        const float d2 = MathUtils::LengthSquared(localEnd - q);
        if (d2 < bestD2) { bestD2 = d2; bestP = localEnd; bestQ = q; }
    }

    // 矩形中心（ローカル原点）から線分への最近接点もサンプルする
    const Vector2 pOnSeg = ClosestPointOnSegment(localSeg, Vector2{0.0f, 0.0f});
    {
        const Vector2 q = ClosestPointOnRectLocal(r.halfSize, pOnSeg);
        const float d2 = MathUtils::LengthSquared(pOnSeg - q);
        if (d2 < bestD2) { bestD2 = d2; bestP = pOnSeg; bestQ = q; }
    }

    const Vector2 d = bestP - bestQ;
    const float rsum = cap.radius;
    if (bestD2 > rsum * rsum) {
        // 線分が矩形内部を貫通している（距離0）場合でもヒット扱いにしたい
        const bool insideLocal = std::abs(pOnSeg.x) <= r.halfSize.x && std::abs(pOnSeg.y) <= r.halfSize.y;
        if (!insideLocal) {
            return MakeNoHit();
        }
    }

    const float dist = std::sqrt(std::max(0.0f, bestD2));
    const Vector2 nLocal = NormalizeSafe(d, Vector2{1.0f, 0.0f});
    return MakeHit(RectLocalDirectionToWorld(r, nLocal), rsum - dist);
}

inline HitInfo ComputeHit(const Math::Rect &r, const Math::Capsule2D &cap) {
    HitInfo hi = ComputeHit(cap, r);
    if (hi.isHit) hi.normal = hi.normal * -1.0f;
    return hi;
}

inline HitInfo ComputeHit(const Math::Capsule2D &a, const Math::Capsule2D &b) {
    // 半径の和を使い、線分-線分（カプセルの中心線同士）として扱う
    float s = 0.0f;
    float t = 0.0f;
    (void)ClosestPointBetweenSegments(a.start, a.end, b.start, b.end, s, t);
    const Vector2 ca = PointOnSegment(a.start, a.end, s);
    const Vector2 cb = PointOnSegment(b.start, b.end, t);
    const Vector2 d = cb - ca;
    const float r = a.radius + b.radius;
    const float dist2 = MathUtils::LengthSquared(d);
    if (dist2 > r * r) return MakeNoHit();
    const float dist = std::sqrt(std::max(0.0f, dist2));
    const Vector2 n = NormalizeSafe(d, Vector2{1.0f, 0.0f});
    return MakeHit(n, r - dist);
}

} // namespace CollisionAlgorithms2D
} // namespace KashipanEngine
