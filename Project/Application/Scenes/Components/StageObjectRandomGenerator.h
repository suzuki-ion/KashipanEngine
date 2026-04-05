#pragma once

#include <KashipanEngine.h>
#include "Objects/Components/GroundDefined.h"

#include <array>

namespace KashipanEngine {

class StageObjectRandomGenerator final : public ISceneComponent {
public:
    StageObjectRandomGenerator() : ISceneComponent("StageObjectRandomGenerator", 1) {}
    ~StageObjectRandomGenerator() override = default;

    void Initialize() override {
        auto *ctx = GetOwnerContext();
        if (!ctx) return;

        auto *defaultVars = ctx->GetComponent<SceneDefaultVariables>();
        if (!defaultVars) return;

        auto *colliderComp = defaultVars->GetColliderComp();
        if (!colliderComp) return;

        Collider *collider = colliderComp->GetCollider();
        if (!collider) return;

        CreateInitialGrounds(ctx, defaultVars, collider);
        CreateRandomGrounds(ctx, defaultVars, collider);
    }

private:
    enum class Side {
        Top,
        Bottom,
        Left,
        Right,
    };

    void CreateInitialGrounds(SceneContext *ctx, SceneDefaultVariables *defaultVars, Collider *collider) {
        constexpr float initialLength = 50.0f;
        constexpr std::array<Side, 4> kSides = {Side::Top, Side::Bottom, Side::Left, Side::Right};

        for (const Side side : kSides) {
            CreateGroundSegment(ctx, defaultVars, collider, side, 0.0f, -initialLength);
        }
    }

    void CreateRandomGrounds(SceneContext *ctx, SceneDefaultVariables *defaultVars, Collider *collider) {
        constexpr std::array<Side, 4> kSides = {Side::Top, Side::Bottom, Side::Left, Side::Right};

        float cursorZ = -50.0f;
        while (cursorZ > -maxStageDepth_) {
            const float segmentLength = GetRandomValue(50.0f, 100.0f);
            const float nextZ = cursorZ - segmentLength;

            const int mask = GetRandomValue(1, 15);
            for (int i = 0; i < static_cast<int>(kSides.size()); ++i) {
                if ((mask & (1 << i)) == 0) continue;
                CreateGroundSegment(ctx, defaultVars, collider, kSides[static_cast<std::size_t>(i)], cursorZ, nextZ);
            }

            cursorZ = nextZ;
        }
    }

    void CreateGroundSegment(
        SceneContext *ctx,
        SceneDefaultVariables *defaultVars,
        Collider *collider,
        Side side,
        float startZ,
        float endZ) {
        if (!ctx || !defaultVars) return;

        auto obj = std::make_unique<Box>();
        obj->SetName("Ground");

        if (defaultVars->GetScreenBuffer3D()) {
            obj->AttachToRenderer(defaultVars->GetScreenBuffer3D(), "Object3D.Solid.BlendNormal");
        }

        auto *tr = obj->GetComponent3D<Transform3D>();
        if (!tr) return;

        const float centerZ = (startZ + endZ) * 0.5f;
        const float length = std::abs(endZ - startZ);

        constexpr float width = 20.0f;
        constexpr float thickness = 1.0f;
        constexpr float distanceFromCenter = 10.5f;

        switch (side) {
        case Side::Top:
            tr->SetTranslate(Vector3{0.0f, distanceFromCenter, centerZ});
            tr->SetScale(Vector3{width, thickness, length});
            break;
        case Side::Bottom:
            tr->SetTranslate(Vector3{0.0f, -distanceFromCenter, centerZ});
            tr->SetScale(Vector3{width, thickness, length});
            break;
        case Side::Left:
            tr->SetTranslate(Vector3{-distanceFromCenter, 0.0f, centerZ});
            tr->SetScale(Vector3{thickness, width, length});
            break;
        case Side::Right:
            tr->SetTranslate(Vector3{distanceFromCenter, 0.0f, centerZ});
            tr->SetScale(Vector3{thickness, width, length});
            break;
        }

        obj->RegisterComponent<GroundDefined>(collider);

        (void)ctx->AddObject3D(std::move(obj));
    }

    float maxStageDepth_ = 8192.0f;
};

} // namespace KashipanEngine
