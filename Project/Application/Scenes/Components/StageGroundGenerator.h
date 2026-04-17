#pragma once

#include <KashipanEngine.h>
#include "Objects/Components/GroundDefined.h"
#include "Objects/Components/SlowGroundDefined.h"

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
        PreparePresets();
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

    void SpawnStartGroundUnderPlayer(const Vector3 &playerPos) {
        if (!spawnGround_) return;
        auto *tr = spawnGround_->GetComponent3D<Transform3D>();
        if (!tr) return;

        tr->SetTranslate(Vector3{playerPos.x, playerPos.y - panelThickness_, playerPos.z});
        tr->SetRotate(Vector3{0.0f, 0.0f, 0.0f});
        tr->SetScale(Vector3{spawnGroundWidth_, panelThickness_, spawnGroundDepth_});

        if (auto *groundDefined = spawnGround_->GetComponent3D<GroundDefined>()) {
            groundDefined->ResetTouchColorAnimation();
        }
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

    struct GenerationPreset {
        float useProbability = 1.0f;

        float minRingRadius = 64.0f;
        float maxRingRadius = 64.0f;
        int ringSplitCount = 3;

        int angleSplitCount = 16;

        float minPanelThickness = 2.0f;
        float maxPanelThickness = 2.0f;
        int panelThicknessSplitCount = 2;

        float minPanelWidth = 32.0f;
        float maxPanelWidth = 32.0f;
        int panelWidthSplitCount = 3;

        int minPanelsPerSegment = 12;
        int maxPanelsPerSegment = 16;
    };

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

    void PreparePresets() {
        generationPresets_.clear();

        GenerationPreset base;
        base.useProbability = 1.0f;
        base.minRingRadius = 64.0f;
        base.maxRingRadius = 64.0f;
        base.ringSplitCount = 1;
        base.angleSplitCount = 16;
        base.minPanelThickness = 2.0f;
        base.maxPanelThickness = 2.0f;
        base.panelThicknessSplitCount = 2;
        base.minPanelWidth = 32.0f;
        base.maxPanelWidth = 32.0f;
        base.panelWidthSplitCount = 3;
        base.minPanelsPerSegment = 15;
        base.maxPanelsPerSegment = 16;
        generationPresets_.push_back(base);

        GenerationPreset center = base;
        center.useProbability = 1.0f;
        center.minRingRadius = 16.0f;
        center.maxRingRadius = 32.0f;
        center.ringSplitCount = 2;
        center.angleSplitCount = 8;
        center.minPanelWidth = 16.0f;
        center.maxPanelWidth = 32.0f;
        center.panelWidthSplitCount = 2;
        center.minPanelsPerSegment = 4;
        center.maxPanelsPerSegment = 8;
        generationPresets_.push_back(center);
    }

    bool SelectPresetBatch() {
        activePresets_.clear();
        activePresetIndex_ = 0;
        currentBatchPositionInitialized_ = false;

        if (generationPresets_.empty()) return false;

        for (const auto &preset : generationPresets_) {
            const float probability = std::clamp(preset.useProbability, 0.0f, 1.0f);
            if (probability <= 0.0f) continue;
            if (GetRandomValue(0.0f, 1.0f) <= probability) {
                activePresets_.push_back(preset);
            }
        }

        if (activePresets_.empty()) {
            activePresets_.push_back(generationPresets_.front());
        }

        return true;
    }

    bool AdvanceToNextPreset() {
        if (generationPresets_.empty()) return false;

        if (activePresets_.empty() || activePresetIndex_ >= activePresets_.size()) {
            if (!SelectPresetBatch()) return false;
        }

        currentPreset_ = activePresets_[activePresetIndex_++];
        return true;
    }

    void ResetAngleSlotsForSegment(int angleSplitCount) {
        segmentAvailableAngleIndices_.clear();
        segmentAvailableAngleIndices_.reserve(static_cast<std::size_t>(std::max(0, angleSplitCount)));
        for (int i = 0; i < angleSplitCount; ++i) {
            segmentAvailableAngleIndices_.push_back(i);
        }
    }

    float ConsumeRandomUniqueSegmentAngle() {
        const int splitCount = std::max(1, currentPreset_.angleSplitCount);
        if (segmentAvailableAngleIndices_.empty()) {
            ResetAngleSlotsForSegment(splitCount);
        }

        const int pick = GetRandomValue(0, static_cast<int>(segmentAvailableAngleIndices_.size()) - 1);
        const int angleIndex = segmentAvailableAngleIndices_[static_cast<std::size_t>(pick)];
        segmentAvailableAngleIndices_.erase(segmentAvailableAngleIndices_.begin() + pick);

        const float t = static_cast<float>(angleIndex) / static_cast<float>(std::max(1, splitCount - 1));
        return 0.0f + (kTwoPi - 0.0f) * t;
    }

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

			// 一部の地面にスロー効果を付与
            if (rand() % 5 == 0) {
                obj->RegisterComponent<SlowGroundDefined>();
            }

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
        spawnGround_ = obj.get();
        (void)ctx->AddObject3D(std::move(obj));
    }

    bool StartNextPreset(GroundRuntime &runtime) {
        if (!AdvanceToNextPreset()) return false;

        if (!currentBatchPositionInitialized_) {
            currentSegmentLength_ = GetRandomSplitValue(
                minPanelLength_,
                maxPanelLength_,
                panelLengthSplitCount_);
            currentSegmentCenterZ_ = nextSpawnZ_ - currentSegmentLength_ * 0.5f;
            nextSpawnZ_ -= currentSegmentLength_;
            currentBatchPositionInitialized_ = true;
        }

        if (hasMinSpawnZ_ && currentSegmentCenterZ_ < minSpawnZ_) {
            if (auto *tr2 = runtime.object->GetComponent3D<Transform3D>()) {
                tr2->SetScale(Vector3{0.0f, 0.0f, 0.0f});
            }
            runtime.centerZ = -1000000000.0f;
            runtime.length = 0.0f;
            return false;
        }

        const int angleSlots = std::max(1, currentPreset_.angleSplitCount);
        int panelCount = GetRandomValue(currentPreset_.minPanelsPerSegment, currentPreset_.maxPanelsPerSegment);
        panelCount = std::clamp(panelCount, 1, angleSlots);
        remainingPanelsInCurrentSegment_ = panelCount;
        ResetAngleSlotsForSegment(angleSlots);
        return true;
    }

    void RespawnGround(GroundRuntime &runtime) {
        if (!runtime.object) return;

        if (auto *groundDefined = runtime.object->GetComponent3D<GroundDefined>()) {
            groundDefined->ResetTouchColorAnimation();
        }

        auto *tr = runtime.object->GetComponent3D<Transform3D>();
        if (!tr) return;

        if (remainingPanelsInCurrentSegment_ <= 0) {
            if (!StartNextPreset(runtime) || remainingPanelsInCurrentSegment_ <= 0) {
                return;
            }
        }

        const float length = currentSegmentLength_;
        const float centerZ = currentSegmentCenterZ_;
        --remainingPanelsInCurrentSegment_;

        const float radius = GetRandomSplitValue(
            currentPreset_.minRingRadius,
            currentPreset_.maxRingRadius,
            currentPreset_.ringSplitCount);
        const float angle = ConsumeRandomUniqueSegmentAngle();
        const float panelWidth = GetRandomSplitValue(
            currentPreset_.minPanelWidth,
            currentPreset_.maxPanelWidth,
            currentPreset_.panelWidthSplitCount);
        const float panelThickness = GetRandomSplitValue(
            currentPreset_.minPanelThickness,
            currentPreset_.maxPanelThickness,
            currentPreset_.panelThicknessSplitCount);

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
    float spawnGroundDepth_ = 512.0f;

    float minPanelLength_ = 128.0f;
    float maxPanelLength_ = 128.0f;
    int panelLengthSplitCount_ = 1;

    float nextSpawnZ_ = 0.0f;
    float currentSegmentCenterZ_ = 0.0f;
    float currentSegmentLength_ = 0.0f;
    int remainingPanelsInCurrentSegment_ = 0;
    bool currentBatchPositionInitialized_ = false;

    SceneDefaultVariables *defaultVars_ = nullptr;
    Collider *collider_ = nullptr;
    Object3DBase *player_ = nullptr;
    Object3DBase *spawnGround_ = nullptr;
    std::vector<GroundRuntime> grounds_{};
    int touchedGroundCount_ = 0;
    bool hasMinSpawnZ_ = false;
    float minSpawnZ_ = -1000000000.0f;

    std::vector<GenerationPreset> generationPresets_{};
    std::vector<GenerationPreset> activePresets_{};
    std::size_t activePresetIndex_ = 0;
    GenerationPreset currentPreset_{};
    std::vector<int> segmentAvailableAngleIndices_{};
};

} // namespace KashipanEngine
