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

inline Vector2 ClosestPointOnRect(const Math::Rect &r, const Vector2 &p) {
    const float x = Clamp(p.x, r.center.x - r.halfSize.x, r.center.x + r.halfSize.x);
    const float y = Clamp(p.y, r.center.y - r.halfSize.y, r.center.y + r.halfSize.y);
    return Vector2{x, y};
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
    const Vector2 d = p.position - r.center;
    return std::abs(d.x) <= r.halfSize.x && std::abs(d.y) <= r.halfSize.y;
}
inline bool Intersects(const Math::Point2D &p, const Math::Rect &r) { return Intersects(r, p); }

inline bool Intersects(const Math::Rect &a, const Math::Rect &b) {
    const Vector2 d = b.center - a.center;
    return std::abs(d.x) <= (a.halfSize.x + b.halfSize.x) && std::abs(d.y) <= (a.halfSize.y + b.halfSize.y);
}

inline bool Intersects(const Math::Circle &c, const Math::Rect &r) {
    const float closestX = Clamp(c.center.x, r.center.x - r.halfSize.x, r.center.x + r.halfSize.x);
    const float closestY = Clamp(c.center.y, r.center.y - r.halfSize.y, r.center.y + r.halfSize.y);
    const Vector2 closest{closestX, closestY};
    return DistanceSquared(c.center, closest) <= c.radius * c.radius;
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
    const Vector2 d = b.center - a.center;
    const float px = (a.halfSize.x + b.halfSize.x) - std::abs(d.x);
    const float py = (a.halfSize.y + b.halfSize.y) - std::abs(d.y);
    if (px < 0.0f || py < 0.0f) return MakeNoHit();

    if (px < py) {
        const float sx = (d.x >= 0.0f) ? 1.0f : -1.0f;
        return MakeHit(Vector2{sx, 0.0f}, px);
    }

    const float sy = (d.y >= 0.0f) ? 1.0f : -1.0f;
    return MakeHit(Vector2{0.0f, sy}, py);
}

inline HitInfo ComputeHit(const Math::Circle &c, const Math::Rect &r) {
    const float closestX = Clamp(c.center.x, r.center.x - r.halfSize.x, r.center.x + r.halfSize.x);
    const float closestY = Clamp(c.center.y, r.center.y - r.halfSize.y, r.center.y + r.halfSize.y);
    const Vector2 closest{closestX, closestY};

    const Vector2 v = c.center - closest;
    const float dist2 = MathUtils::LengthSquared(v);
    const float rr = c.radius * c.radius;
    if (dist2 > rr) return MakeNoHit();

    const float dist = std::sqrt(std::max(0.0f, dist2));

    // If center is inside rect, pick minimal axis push-out
    const bool inside = (c.center.x >= r.center.x - r.halfSize.x && c.center.x <= r.center.x + r.halfSize.x) &&
                        (c.center.y >= r.center.y - r.halfSize.y && c.center.y <= r.center.y + r.halfSize.y);

    if (inside) {
        const float left = c.center.x - (r.center.x - r.halfSize.x);
        const float right = (r.center.x + r.halfSize.x) - c.center.x;
        const float down = c.center.y - (r.center.y - r.halfSize.y);
        const float up = (r.center.y + r.halfSize.y) - c.center.y;

        float minAxis = left;
        Vector2 n{-1.0f, 0.0f};
        if (right < minAxis) { minAxis = right; n = Vector2{1.0f, 0.0f}; }
        if (down < minAxis) { minAxis = down; n = Vector2{0.0f, -1.0f}; }
        if (up < minAxis) { minAxis = up; n = Vector2{0.0f, 1.0f}; }

        return MakeHit(n, c.radius + minAxis);
    }

    const Vector2 n = NormalizeSafe(v, Vector2{1.0f, 0.0f});
    const float penetration = c.radius - dist;
    return MakeHit(n, penetration);
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
    if (!Intersects(r, p)) return MakeNoHit();
    const Vector2 d = p.position - r.center;
    const float px = r.halfSize.x - std::abs(d.x);
    const float py = r.halfSize.y - std::abs(d.y);
    if (px < py) {
        const float sx = (d.x >= 0.0f) ? 1.0f : -1.0f;
        return MakeHit(Vector2{sx, 0.0f}, px);
    }
    const float sy = (d.y >= 0.0f) ? 1.0f : -1.0f;
    return MakeHit(Vector2{0.0f, sy}, py);
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
    const Math::Segment2D seg{cap.start, cap.end};
    // 矩形に対して、各端点および線分自体の最近接点を使って近似的に判定する
    Vector2 bestP = cap.start;
    Vector2 bestQ = ClosestPointOnRect(r, cap.start);
    float bestD2 = MathUtils::LengthSquared(bestP - bestQ);

    {
        const Vector2 q = ClosestPointOnRect(r, cap.end);
        const float d2 = MathUtils::LengthSquared(cap.end - q);
        if (d2 < bestD2) { bestD2 = d2; bestP = cap.end; bestQ = q; }
    }

    // 矩形中心から線分への最近接点もサンプルする
    {
        const Vector2 pOnSeg = ClosestPointOnSegment(seg, r.center);
        const Vector2 q = ClosestPointOnRect(r, pOnSeg);
        const float d2 = MathUtils::LengthSquared(pOnSeg - q);
        if (d2 < bestD2) { bestD2 = d2; bestP = pOnSeg; bestQ = q; }
    }

    const Vector2 d = bestP - bestQ;
    const float rsum = cap.radius;
    if (bestD2 > rsum * rsum) {
        // 線分が矩形内部を貫通している（距離0）場合でもヒット扱いにしたい
        if (!Intersects(r, Math::Point2D{ClosestPointOnSegment(seg, r.center)})) {
            return MakeNoHit();
        }
    }

    const float dist = std::sqrt(std::max(0.0f, bestD2));
    const Vector2 n = NormalizeSafe(d, Vector2{1.0f, 0.0f});
    return MakeHit(n, rsum - dist);
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
