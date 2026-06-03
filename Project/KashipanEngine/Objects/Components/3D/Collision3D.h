#pragma once

#include "Objects/IObjectComponent.h"
#include "Objects/Collision/Collider.h"
#include "Objects/ObjectContext.h"
#include "Objects/Components/3D/Transform3D.h"
#include "Utilities/MathUtils.h"
#include <reactphysics3d/reactphysics3d.h>

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

                const auto transformPoint = [&](const Vector3 &p) {
                    return MathUtils::Transform(p, world);
                };

                const auto transformDir = [&](const Vector3 &v) {
                    Vector3 outV{};
                    outV.x = v.x * world.m[0][0] + v.y * world.m[1][0] + v.z * world.m[2][0];
                    outV.y = v.x * world.m[0][1] + v.y * world.m[1][1] + v.z * world.m[2][1];
                    outV.z = v.x * world.m[0][2] + v.y * world.m[1][2] + v.z * world.m[2][2];
                    return outV;
                };

                if constexpr (std::is_same_v<S, ColliderInfo3D::SphereShape3D>) {
                    ColliderInfo3D::SphereShape3D sp = sh;
                    sp.center = transformPoint(sp.center);

                    const Vector3 ax = transformDir(Vector3{1.0f, 0.0f, 0.0f});
                    const Vector3 ay = transformDir(Vector3{0.0f, 1.0f, 0.0f});
                    const Vector3 az = transformDir(Vector3{0.0f, 0.0f, 1.0f});
                    const float rs = std::max({ax.Length(), ay.Length(), az.Length()});
                    sp.radius = sp.radius * rs;
                    return sp;
                } else if constexpr (std::is_same_v<S, ColliderInfo3D::BoxShape3D>) {
                    ColliderInfo3D::BoxShape3D b = sh;
                    b.center = transformPoint(b.center);

                    const Vector3 ax = transformDir(Vector3{1.0f, 0.0f, 0.0f});
                    const Vector3 ay = transformDir(Vector3{0.0f, 1.0f, 0.0f});
                    const Vector3 az = transformDir(Vector3{0.0f, 0.0f, 1.0f});
                    b.halfExtents = Vector3{b.halfExtents.x * ax.Length(), b.halfExtents.y * ay.Length(), b.halfExtents.z * az.Length()};
                    return b;
                } else if constexpr (std::is_same_v<S, ColliderInfo3D::CapsuleShape3D>) {
                    ColliderInfo3D::CapsuleShape3D cap = sh;
                    cap.center = transformPoint(cap.center);

                    const Vector3 ax = transformDir(Vector3{1.0f, 0.0f, 0.0f});
                    const Vector3 ay = transformDir(Vector3{0.0f, 1.0f, 0.0f});
                    const Vector3 az = transformDir(Vector3{0.0f, 0.0f, 1.0f});
                    const float rs = std::max({ax.Length(), ay.Length(), az.Length()});
                    cap.radius = cap.radius * rs;
                    cap.height = cap.height * ay.Length();
                    return cap;
                } else if constexpr (std::is_same_v<S, ColliderInfo3D::ConvexMeshShape3D>) {
                    ColliderInfo3D::ConvexMeshShape3D mesh = sh;
                    const Vector3 ax = transformDir(Vector3{1.0f, 0.0f, 0.0f});
                    const Vector3 ay = transformDir(Vector3{0.0f, 1.0f, 0.0f});
                    const Vector3 az = transformDir(Vector3{0.0f, 0.0f, 1.0f});
                    mesh.scale = Vector3{mesh.scale.x * ax.Length(), mesh.scale.y * ay.Length(), mesh.scale.z * az.Length()};
                    return mesh;
                } else if constexpr (std::is_same_v<S, ColliderInfo3D::HeightFieldShape3D>) {
                    ColliderInfo3D::HeightFieldShape3D hf = sh;
                    const Vector3 ax = transformDir(Vector3{1.0f, 0.0f, 0.0f});
                    const Vector3 ay = transformDir(Vector3{0.0f, 1.0f, 0.0f});
                    const Vector3 az = transformDir(Vector3{0.0f, 0.0f, 1.0f});
                    hf.scale = Vector3{hf.scale.x * ax.Length(), hf.scale.y * ay.Length(), hf.scale.z * az.Length()};
                    hf.minHeight = hf.minHeight * ay.Length();
                    hf.maxHeight = hf.maxHeight * ay.Length();
                    return hf;
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
