#pragma once
#pragma once

#include <KashipanEngine.h>
#include "Objects/Components/GroundDefined.h"

#include <algorithm>
#include <cmath>
#include <cstdint>
#include <vector>

namespace KashipanEngine {

class StageGroundGenerator final : public ISceneComponent {
public:
    StageGroundGenerator() : ISceneComponent("StageGroundGenerator", 1) {}
    ~StageGroundGenerator() override = default;

    void Initialize() override {
        auto *ctx = GetOwnerContext();
        if (!ctx) return;

        defaultVars_ = ctx->GetComponent<SceneDefaultVariables>();
        if (!defaultVars_) return;

        auto *colliderComp = defaultVars_->GetColliderComp();
        if (!colliderComp) return;

        collider_ = colliderComp->GetCollider();
        TryGenerate();
    }

    void Update() override {
        if (!generated_ || grounds_.empty()) return;

        auto *ctx = GetOwnerContext();
        if (!ctx) return;

        if (!player_) {
            player_ = ctx->GetObject3D("PlayerRoot");
        }
        if (!player_) return;

        auto *playerTr = player_->GetComponent3D<Transform3D>();
        if (!playerTr) return;

        const float playerZ = playerTr->GetTranslate().z;
        for (auto &g : grounds_) {
            if (!g.object) continue;

            if (auto *ground = g.object->GetComponent3D<GroundDefined>()) {
                if (ground->ConsumePlayerTouchEvent()) {
                    ++touchedGroundCount_;
                }
            }

            if (g.centerZ > playerZ + recycleBehindDistance_) {
                RespawnGround(g);
            }
        }
    }

    void RequestGenerate() {
        requested_ = true;
        TryGenerate();
    }

    void TriggerGroundReaction(const Vector3 &center, float radius) {
        const float radiusSq = std::max(0.0f, radius) * std::max(0.0f, radius);
        for (auto &g : grounds_) {
            if (!g.object) continue;
            auto *tr = g.object->GetComponent3D<Transform3D>();
            auto *ground = g.object->GetComponent3D<GroundDefined>();
            if (!tr || !ground) continue;
            const Vector3 d = tr->GetTranslate() - center;
            if (d.LengthSquared() <= radiusSq) {
                ground->TriggerTouchColorAnimation();
            }
        }
    }

    int GetTouchedGroundCount() const { return touchedGroundCount_; }

    void SetMinSpawnZ(float minSpawnZ) {
        minSpawnZ_ = minSpawnZ;
        hasMinSpawnZ_ = true;
    }

private:
    static constexpr float kTwoPi = 3.14159265358979323846f * 2.0f;
    static constexpr std::uint64_t kGroundBatchKey = 0x1101000000000001ull;

    static float GetRandomSplitValue(float minValue, float maxValue, int splitCount) {
        if (splitCount <= 1) {
            return GetRandomValue(minValue, maxValue);
        }
        const int index = GetRandomValue(0, std::max(0, splitCount - 1));
        const float t = static_cast<float>(index) / static_cast<float>(std::max(1, splitCount - 1));
        return minValue + (maxValue - minValue) * t;
    }

    struct GroundRuntime {
        Object3DBase *object = nullptr;
        float centerZ = 0.0f;
        float length = 0.0f;
    };

    void TryGenerate() {
        if (generated_ || !requested_) return;

        auto *ctx = GetOwnerContext();
        if (!ctx || !defaultVars_ || !collider_) return;

        CreateSpawnGround(ctx);
        CreateGroundPool(ctx);
        generated_ = true;
    }

    void CreateGroundPool(SceneContext *ctx) {
        grounds_.clear();
        grounds_.reserve(static_cast<std::size_t>(pooledGroundObjectCount_));

        nextSpawnZ_ = spawnGroundCenterZ_ - spawnGroundDepth_ * 0.5f;

        for (int i = 0; i < pooledGroundObjectCount_; ++i) {
            auto obj = std::make_unique<Box>();
            obj->SetName("Ground");
            obj->SetBatchKey(kGroundBatchKey, RenderType::Instancing);

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

    void CreateSpawnGround(SceneContext *ctx) {
        if (!ctx || !defaultVars_ || !collider_) return;

        auto obj = std::make_unique<Box>();
        obj->SetName("Ground");
        obj->SetBatchKey(kGroundBatchKey, RenderType::Instancing);

        if (defaultVars_->GetScreenBuffer3D()) {
            obj->AttachToRenderer(defaultVars_->GetScreenBuffer3D(), "Object3D.Solid.BlendNormal");
        }

        auto *tr = obj->GetComponent3D<Transform3D>();
        if (!tr) return;

        tr->SetTranslate(Vector3{spawnGroundCenterX_, spawnGroundCenterY_, spawnGroundCenterZ_});
        tr->SetRotate(Vector3{0.0f, 0.0f, 0.0f});
        tr->SetScale(Vector3{spawnGroundWidth_, panelThickness_, spawnGroundDepth_});

        obj->RegisterComponent<GroundDefined>(collider_);
        (void)ctx->AddObject3D(std::move(obj));
    }

    void RespawnGround(GroundRuntime &runtime) {
        if (!runtime.object) return;

        if (auto *groundDefined = runtime.object->GetComponent3D<GroundDefined>()) {
            groundDefined->ResetTouchColorAnimation();
        }

        auto *tr = runtime.object->GetComponent3D<Transform3D>();
        if (!tr) return;

        if (remainingPanelsInCurrentSegment_ <= 0) {
            currentSegmentLength_ = GetRandomSplitValue(minPanelLength_, maxPanelLength_, panelLengthSplitCount_);
            currentSegmentCenterZ_ = nextSpawnZ_ - currentSegmentLength_ * 0.5f;

            if (hasMinSpawnZ_ && currentSegmentCenterZ_ < minSpawnZ_) {
                if (auto *tr2 = runtime.object->GetComponent3D<Transform3D>()) {
                    tr2->SetScale(Vector3{0.0f, 0.0f, 0.0f});
                }
                runtime.centerZ = -1000000000.0f;
                runtime.length = 0.0f;
                return;
            }

            nextSpawnZ_ -= currentSegmentLength_;
            remainingPanelsInCurrentSegment_ = GetRandomValue(minPanelsPerSegment_, maxPanelsPerSegment_);
        }

        const float length = currentSegmentLength_;
        const float centerZ = currentSegmentCenterZ_;
        --remainingPanelsInCurrentSegment_;

        const float radius = GetRandomSplitValue(minRingRadius_, maxRingRadius_, ringSplitCount_);
        const float angle = GetRandomValue(0.0f, kTwoPi);
        const float panelWidth = GetRandomSplitValue(minPanelWidth_, maxPanelWidth_, panelWidthSplitCount_);
        const float panelThickness = GetRandomSplitValue(minPanelThickness_, maxPanelThickness_, panelThicknessSplitCount_);

        const float x = std::cos(angle) * radius;
        const float y = std::sin(angle) * radius;

        tr->SetTranslate(Vector3{x, y, centerZ});
        tr->SetRotate(Vector3{0.0f, 0.0f, angle});
        tr->SetScale(Vector3{panelThickness, panelWidth, length});

        runtime.centerZ = centerZ;
        runtime.length = length;
    }

    bool requested_ = false;
    bool generated_ = false;

    float panelThickness_ = 2.0f;
    int pooledGroundObjectCount_ = 512;
    float recycleBehindDistance_ = 1024.0f;

    float spawnGroundCenterX_ = 0.0f;
    float spawnGroundCenterY_ = -panelThickness_;
    float spawnGroundCenterZ_ = -2.0f;
    float spawnGroundWidth_ = 16.0f;
    float spawnGroundDepth_ = 256.0f;

    float minRingRadius_ = 16.0f;
    float maxRingRadius_ = 96.0f;
    int ringSplitCount_ = 4;

    float minPanelThickness_ = 1.0f;
    float maxPanelThickness_ = 8.0f;
    int panelThicknessSplitCount_ = 2;

    float minPanelWidth_ = 16.0f;
    float maxPanelWidth_ = 32.0f;
    int panelWidthSplitCount_ = 3;

    float minPanelLength_ = 16.0f;
    float maxPanelLength_ = 64.0f;
    int panelLengthSplitCount_ = 3;

    int minPanelsPerSegment_ = 4;
    int maxPanelsPerSegment_ = 8;

    float nextSpawnZ_ = 0.0f;
    float currentSegmentCenterZ_ = 0.0f;
    float currentSegmentLength_ = 0.0f;
    int remainingPanelsInCurrentSegment_ = 0;

    SceneDefaultVariables *defaultVars_ = nullptr;
    Collider *collider_ = nullptr;
    Object3DBase *player_ = nullptr;
    std::vector<GroundRuntime> grounds_{};
    int touchedGroundCount_ = 0;
    bool hasMinSpawnZ_ = false;
    float minSpawnZ_ = -1000000000.0f;
};

} // namespace KashipanEngine
