#pragma once

#include "Objects/IObjectComponent.h"
#include "Objects/Collision/Collider.h"
#include "Objects/ObjectContext.h"
#include "Objects/Components/2D/Transform2D.h"

#include <memory>
#include <optional>
#include <algorithm>
#include <type_traits>

namespace KashipanEngine {

class Collision2D final : public IObjectComponent2D {
public:
    Collision2D(Collider *collider, const ColliderInfo2D &info = {})
        : IObjectComponent2D("Collision2D", 100), collider_(collider), worldInfo_(info) {
        localInfo_ = worldInfo_;
    }

    ~Collision2D() override = default;

    std::unique_ptr<IObjectComponent> Clone() const override {
        auto ptr = std::make_unique<Collision2D>(collider_, localInfo_);
        return ptr;
    }

    std::optional<bool> Initialize() override {
        if (!collider_) return false;
        if (colliderId_ != 0) return true;

        if (auto *ctx = GetOwner2DContext()) {
            worldInfo_.ownerObject = ctx->GetOwner();
        }

        worldInfo_ = MakeWorldInfo();
        colliderId_ = collider_->Add(worldInfo_);
        return true;
    }

    std::optional<bool> Finalize() override {
        if (collider_ && colliderId_ != 0) {
            collider_->Remove2D(colliderId_);
        }
        colliderId_ = 0;
        return true;
    }

    std::optional<bool> Update() override {
        if (!collider_ || colliderId_ == 0) return true;
        worldInfo_ = MakeWorldInfo();
        collider_->UpdateColliderInfo2D(colliderId_, worldInfo_);
        return true;
    }

    const ColliderInfo2D &GetInfo() const { return localInfo_; }
    ColliderInfo2D &GetInfo() { return localInfo_; }

    Collider *GetCollider() const { return collider_; }
    Collider::ColliderID GetColliderID() const { return colliderId_; }

    void SetCollider(Collider *collider) { collider_ = collider; }

#if defined(USE_IMGUI)
    void ShowImGui() override {}
#endif

private:
    ColliderInfo2D MakeWorldInfo() const {
        ColliderInfo2D out = localInfo_;
        out.ownerObject = worldInfo_.ownerObject;

        auto *tr = GetOwner2DContext() ? GetOwner2DContext()->GetComponent<Transform2D>() : nullptr;
        if (!tr) {
            out.shape = localInfo_.shape;
            return out;
        }

        // Transform のワールド行列で当たり判定形状を更新する
        const Matrix4x4 &world = tr->GetWorldMatrix();

        const auto transformPoint2 = [&](const Vector2 &p) {
            const Vector3 tp = MathUtils::Transform(Vector3{p.x, p.y, 0.0f}, world);
            return Vector2{tp.x, tp.y};
        };

        // 回転/スケールを含めて方向ベクトルを変換する（平行移動を除外）
        const auto transformDir2 = [&](const Vector2 &v) {
            Vector3 outV{};
            outV.x = v.x * world.m[0][0] + v.y * world.m[1][0];
            outV.y = v.x * world.m[0][1] + v.y * world.m[1][1];
            return Vector2{outV.x, outV.y};
        };

        out.shape = std::visit(
            [&](const auto &sh) -> ColliderInfo2D::ShapeVariant {
                using S = std::decay_t<decltype(sh)>;
                if constexpr (std::is_same_v<S, Math::Point2D>) {
                    Math::Point2D p = sh;
                    p.position = transformPoint2(p.position);
                    return p;
                } else if constexpr (std::is_same_v<S, Math::Circle>) {
                    Math::Circle c = sh;
                    c.center = transformPoint2(c.center);

                    // 半径はワールド行列の軸スケールの最大値で拡大する
                    const Vector2 ax = transformDir2(Vector2{1.0f, 0.0f});
                    const Vector2 ay = transformDir2(Vector2{0.0f, 1.0f});
                    const float rs = std::max(ax.Length(), ay.Length());
                    c.radius = c.radius * rs;
                    return c;
                } else if constexpr (std::is_same_v<S, Math::Rect>) {
                    // Rect は回転を持たないため、ワールド回転がある場合は厳密には OBB 相当になる
                    // 現状の形状型を維持するため、中心だけをワールド変換し、halfSize は軸スケールで拡大する
                    Math::Rect r = sh;
                    r.center = transformPoint2(r.center);
                    const Vector2 ax = transformDir2(Vector2{1.0f, 0.0f});
                    const Vector2 ay = transformDir2(Vector2{0.0f, 1.0f});
                    r.halfSize = Vector2{r.halfSize.x * ax.Length(), r.halfSize.y * ay.Length()};
                    return r;
                } else if constexpr (std::is_same_v<S, Math::Segment2D>) {
                    Math::Segment2D seg = sh;
                    seg.start = transformPoint2(seg.start);
                    seg.end = transformPoint2(seg.end);
                    return seg;
                } else if constexpr (std::is_same_v<S, Math::Capsule2D>) {
                    Math::Capsule2D cap = sh;
                    cap.start = transformPoint2(cap.start);
                    cap.end = transformPoint2(cap.end);

                    const Vector2 ax = transformDir2(Vector2{1.0f, 0.0f});
                    const Vector2 ay = transformDir2(Vector2{0.0f, 1.0f});
                    const float rs = std::max(ax.Length(), ay.Length());
                    cap.radius = cap.radius * rs;
                    return cap;
                } else {
                    return sh;
                }
            },
            localInfo_.shape);

        return out;
    }

    Collider *collider_ = nullptr;
    ColliderInfo2D localInfo_{};
    ColliderInfo2D worldInfo_{};
    Collider::ColliderID colliderId_ = 0;
};

} // namespace KashipanEngine
