#pragma once

#include <KashipanEngine.h>
#include "Objects/Components/GroundDefined.h"

#include <cmath>
#include <vector>

namespace KashipanEngine {

class StageObjectRandomGenerator final : public ISceneComponent {
public:
    StageObjectRandomGenerator() : ISceneComponent("StageObjectRandomGenerator", 1) {}
    ~StageObjectRandomGenerator() override = default;

    void Initialize() override {
        auto *ctx = GetOwnerContext();
        if (!ctx) return;

        defaultVars_ = ctx->GetComponent<SceneDefaultVariables>();
        if (!defaultVars_) return;

        auto *colliderComp = defaultVars_->GetColliderComp();
        if (!colliderComp) return;

        collider_ = colliderComp->GetCollider();
        if (!collider_) return;

        CreateGroundPool(ctx);
    }

    void Update() override {
        auto *ctx = GetOwnerContext();
        if (!ctx || grounds_.empty()) return;

        if (!player_) {
            player_ = ctx->GetObject3D("Player");
        }
        if (!player_) return;

        auto *playerTr = player_->GetComponent3D<Transform3D>();
        if (!playerTr) return;

        const float playerZ = playerTr->GetTranslate().z;
        for (auto &g : grounds_) {
            if (!g.object) continue;

            if (g.centerZ > playerZ + recycleBehindDistance_) {
                RespawnGround(g);
            }
        }
    }

private:
    static constexpr float kPi = 3.14159265358979323846f;
    static constexpr float kTwoPi = kPi * 2.0f;

    struct GroundRuntime {
        Object3DBase *object = nullptr;
        float centerZ = 0.0f;
        float length = 0.0f;
    };

    void CreateGroundPool(SceneContext *ctx) {
        grounds_.clear();
        grounds_.reserve(static_cast<std::size_t>(pooledGroundObjectCount_));

        nextSpawnZ_ = initialSpawnStartZ_;

        for (int i = 0; i < pooledGroundObjectCount_; ++i) {
            auto obj = std::make_unique<Box>();
            obj->SetName("Ground");

            if (defaultVars_ && defaultVars_->GetScreenBuffer3D()) {
                obj->AttachToRenderer(defaultVars_->GetScreenBuffer3D(), "Object3D.Solid.BlendNormal");
            }

            obj->RegisterComponent<GroundDefined>(collider_);

            Object3DBase *objPtr = obj.get();
            if (!ctx->AddObject3D(std::move(obj)) || objPtr == nullptr) {
                continue;
            }

            GroundRuntime runtime{};
            runtime.object = objPtr;
            RespawnGround(runtime);
            grounds_.push_back(runtime);
        }
    }

    void RespawnGround(GroundRuntime &runtime) {
        if (!runtime.object) return;

        auto *tr = runtime.object->GetComponent3D<Transform3D>();
        if (!tr) return;

        if (remainingPanelsInCurrentSegment_ <= 0) {
            currentSegmentLength_ = GetRandomValue(minPanelLength_, maxPanelLength_);
            currentSegmentCenterZ_ = nextSpawnZ_ - currentSegmentLength_ * 0.5f;
            nextSpawnZ_ -= currentSegmentLength_;
            remainingPanelsInCurrentSegment_ = GetRandomValue(minPanelsPerSegment_, maxPanelsPerSegment_);
        }

        const float length = currentSegmentLength_;
        const float centerZ = currentSegmentCenterZ_;
        --remainingPanelsInCurrentSegment_;

        const float radius = GetRandomValue(minRingRadius_, maxRingRadius_);
        const float angle = GetRandomValue(0.0f, kTwoPi);
        const float panelWidth = GetRandomValue(minPanelWidth_, maxPanelWidth_);

        const float x = std::cos(angle) * radius;
        const float y = std::sin(angle) * radius;

        tr->SetTranslate(Vector3{x, y, centerZ});
        tr->SetRotate(Vector3{0.0f, 0.0f, angle});
        tr->SetScale(Vector3{panelThickness_, panelWidth, length});

        runtime.centerZ = centerZ;
        runtime.length = length;
    }

    float panelThickness_ = 1.0f;

    int pooledGroundObjectCount_ = 512;
    float initialSpawnStartZ_ = 0.0f;
    float recycleBehindDistance_ = 64.0f;

    float minRingRadius_ = 8.0f;
    float maxRingRadius_ = 32.0f;
    float minPanelWidth_ = 8.0f;
    float maxPanelWidth_ = 32.0f;
    float minPanelLength_ = 50.0f;
    float maxPanelLength_ = 100.0f;

    int minPanelsPerSegment_ = 1;
    int maxPanelsPerSegment_ = 4;

    float nextSpawnZ_ = 0.0f;
    float currentSegmentCenterZ_ = 0.0f;
    float currentSegmentLength_ = 0.0f;
    int remainingPanelsInCurrentSegment_ = 0;

    SceneDefaultVariables *defaultVars_ = nullptr;
    Collider *collider_ = nullptr;
    Object3DBase *player_ = nullptr;
    std::vector<GroundRuntime> grounds_{};
};

} // namespace KashipanEngine
