#pragma once

#include "Objects/IObjectComponent.h"
#include "Objects/Collision/Collider.h"
#include "Objects/ObjectContext.h"
#include "Objects/Components/3D/Transform3D.h"
#include "Utilities/MathUtils.h"

#include <memory>
#include <optional>
#include <algorithm>
#include <cmath>

namespace KashipanEngine {

class Collision3D final : public IObjectComponent3D {
public:
    Collision3D(Collider *collider, const ColliderInfo3D &info = {})
        : IObjectComponent3D("Collision3D", 100), collider_(collider), worldInfo_(info) {
        localInfo_ = worldInfo_;
    }

    ~Collision3D() override = default;

    std::unique_ptr<IObjectComponent> Clone() const override {
        auto ptr = std::make_unique<Collision3D>(collider_, localInfo_);
        return ptr;
    }

    std::optional<bool> Initialize() override {
        if (!collider_) return false;
        if (colliderId_ != 0) return true;

        if (auto *ctx = GetOwner3DContext()) {
            worldInfo_.ownerObject = ctx->GetOwner();
        }

        worldInfo_ = MakeWorldInfo();
        colliderId_ = collider_->Add(worldInfo_);
        return true;
    }

    std::optional<bool> Finalize() override {
        if (collider_ && colliderId_ != 0) {
            collider_->Remove3D(colliderId_);
        }
        colliderId_ = 0;
        return true;
    }

    std::optional<bool> Update() override {
        if (!collider_ || colliderId_ == 0) return true;
        worldInfo_ = MakeWorldInfo();
        collider_->UpdateColliderInfo3D(colliderId_, worldInfo_);
        return true;
    }

    const ColliderInfo3D &GetColliderInfo() const { return localInfo_; }
    ColliderInfo3D &GetColliderInfo() { return localInfo_; }

    Collider *GetCollider() const { return collider_; }
    Collider::ColliderID GetColliderID() const { return colliderId_; }

    void SetCollider(Collider *collider) { collider_ = collider; }

#if defined(USE_IMGUI)
    void ShowImGui() override {}
#endif

private:
    ColliderInfo3D MakeWorldInfo() const {
        ColliderInfo3D out = localInfo_;
        out.ownerObject = worldInfo_.ownerObject;

        auto *tr = GetOwner3DContext() ? GetOwner3DContext()->GetComponent<Transform3D>() : nullptr;
        if (!tr) {
            out.shape = localInfo_.shape;
            return out;
        }

        // Transform のワールド行列で当たり判定形状を更新する
        const Matrix4x4 &world = tr->GetWorldMatrix();

        out.shape = std::visit(
            [&](const auto &sh) -> ColliderInfo3D::ShapeVariant {
                using S = std::decay_t<decltype(sh)>;

                // 位置だけをワールド変換する（W=1）
                const auto transformPoint = [&](const Vector3 &p) {
                    return MathUtils::Transform(p, world);
                };

                // 回転/スケールを含めて方向ベクトルをワールド変換する（平行移動を除外）
                const auto transformDir = [&](const Vector3 &v) {
                    Vector3 outV{};
                    outV.x = v.x * world.m[0][0] + v.y * world.m[1][0] + v.z * world.m[2][0];
                    outV.y = v.x * world.m[0][1] + v.y * world.m[1][1] + v.z * world.m[2][1];
                    outV.z = v.x * world.m[0][2] + v.y * world.m[1][2] + v.z * world.m[2][2];
                    return outV;
                };

                if constexpr (std::is_same_v<S, Math::Point3D>) {
                    Math::Point3D p = sh;
                    p.position = transformPoint(p.position);
                    return p;
                } else if constexpr (std::is_same_v<S, Math::Sphere>) {
                    Math::Sphere sp = sh;
                    sp.center = transformPoint(sp.center);

                    // 半径はワールド行列の軸スケールの最大値で拡大する
                    const Vector3 ax = transformDir(Vector3{1.0f, 0.0f, 0.0f});
                    const Vector3 ay = transformDir(Vector3{0.0f, 1.0f, 0.0f});
                    const Vector3 az = transformDir(Vector3{0.0f, 0.0f, 1.0f});
                    const float rs = std::max({ax.Length(), ay.Length(), az.Length()});
                    sp.radius = sp.radius * rs;
                    return sp;
                } else if constexpr (std::is_same_v<S, Math::AABB>) {
                    // ローカル AABB -> ワールド座標系で axis-aligned AABB に変換（回転は無視して位置+スケールのみ反映）
                    Math::AABB b = sh;

                    // ローカル中心と halfSize を求める
                    Vector3 localCenter = (b.min + b.max) * 0.5f;
                    Vector3 localHalf = (b.max - b.min) * 0.5f;

                    // 中心はワールド変換（translation, rotation, scale を含む）
                    Vector3 worldCenter = transformPoint(localCenter);

                    // 各軸方向のワールドスケール（回転成分を含むので長さを使う）
                    const Vector3 ax = transformDir(Vector3{1.0f, 0.0f, 0.0f});
                    const Vector3 ay = transformDir(Vector3{0.0f, 1.0f, 0.0f});
                    const Vector3 az = transformDir(Vector3{0.0f, 0.0f, 1.0f});
                    const float sx = std::abs(ax.Length());
                    const float sy = std::abs(ay.Length());
                    const float sz = std::abs(az.Length());

                    // halfSize にそれぞれのスケールを適用（スケールは一回だけ）
                    Vector3 worldHalf = Vector3{ localHalf.x * sx, localHalf.y * sy, localHalf.z * sz };

                    // axis-aligned な min/max を再構築（ワールド軸に沿った AABB）
                    Math::AABB outA;
                    outA.min = worldCenter - worldHalf;
                    outA.max = worldCenter + worldHalf;

                    return outA;
                } else if constexpr (std::is_same_v<S, Math::OBB>) {
                    Math::OBB o = sh;
                    o.center = transformPoint(o.center);

                    const Vector3 ax = transformDir(Vector3{1.0f, 0.0f, 0.0f});
                    const Vector3 ay = transformDir(Vector3{0.0f, 1.0f, 0.0f});
                    const Vector3 az = transformDir(Vector3{0.0f, 0.0f, 1.0f});
                    o.halfSize = Vector3{o.halfSize.x * ax.Length(), o.halfSize.y * ay.Length(), o.halfSize.z * az.Length()};

                    // ローカル orientation にワールドの回転（＋スケール）を合成する
                    Matrix4x4 wNoT = world;
                    wNoT.m[3][0] = 0.0f;
                    wNoT.m[3][1] = 0.0f;
                    wNoT.m[3][2] = 0.0f;
                    wNoT.m[0][3] = 0.0f;
                    wNoT.m[1][3] = 0.0f;
                    wNoT.m[2][3] = 0.0f;
                    wNoT.m[3][3] = 1.0f;
                    o.orientation = wNoT * o.orientation;
                    return o;
                } else if constexpr (std::is_same_v<S, Math::Plane>) {
                    // Plane は現状 Transform を反映しない（必要になった段階で対応する）
                    return sh;
                } else {
                    return sh;
                }
            },
            localInfo_.shape);

        return out;
    }

    Collider *collider_ = nullptr;
    ColliderInfo3D localInfo_{};
    ColliderInfo3D worldInfo_{};
    Collider::ColliderID colliderId_ = 0;
};

} // namespace KashipanEngine
