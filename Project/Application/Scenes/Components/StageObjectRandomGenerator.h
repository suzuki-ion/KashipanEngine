#pragma once
#pragma once

#include <KashipanEngine.h>
#include "Objects/Components/AlwaysRotate.h"
#include "Objects/Components/GroundDefined.h"

#include <cmath>
#include <cstdint>
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

        CreateSpawnGround(ctx);
        CreateGroundPool(ctx);
        CreateDecorationPools(ctx);
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

        const Vector3 playerPos = playerTr->GetTranslate();
        const float playerZ = playerTr->GetTranslate().z;
        for (auto &g : grounds_) {
            if (!g.object) continue;

            if (g.centerZ > playerZ + recycleBehindDistance_) {
                RespawnGround(g);
            }
        }

        for (auto &p : decoPlanes_) {
            if (!p.object) continue;

            if (auto *tr = p.object->GetComponent3D<Transform3D>()) {
                const float dist = (tr->GetTranslate() - playerPos).Length();
                const float alphaT = std::clamp((dist - decoPlaneNearFadeDistance_) / std::max(0.0001f, (decoPlaneFarFadeDistance_ - decoPlaneNearFadeDistance_)), 0.0f, 1.0f);
                const float alpha = 1.0f - alphaT;
                if (auto *mat = p.object->GetComponent3D<Material3D>()) {
                    mat->SetColor(Vector4{0.5f, 1.0f, 0.5f, alpha});
                }
            }

            if (p.centerZ > playerZ + recycleBehindDistance_) {
                RespawnDecoPlane(p);
            }
        }

        for (auto &b : decoBoxes_) {
            if (!b.object) continue;

            if (auto *tr = b.object->GetComponent3D<Transform3D>()) {
                const float dist = (tr->GetTranslate() - playerPos).Length();
                const float alphaT = std::clamp((dist - decoBoxNearFadeDistance_) / std::max(0.0001f, (decoBoxFarFadeDistance_ - decoPlaneNearFadeDistance_)), 0.0f, 1.0f);
                const float alpha = 1.0f - alphaT;
                if (auto *mat = b.object->GetComponent3D<Material3D>()) {
                    mat->SetColor(Vector4{ 0.5f, 1.0f, 0.5f, alpha });
                }
            }

            if (b.centerZ > playerZ + recycleBehindDistance_) {
                RespawnDecoBox(b);
            }
        }
    }

private:
    static constexpr float kPi = 3.14159265358979323846f;
    static constexpr float kTwoPi = kPi * 2.0f;
    static constexpr std::uint64_t kGroundBatchKey = 0x1101000000000001ull;
    static constexpr std::uint64_t kDecoPlaneBatchKey = 0x1101000000000002ull;
    static constexpr std::uint64_t kDecoBoxBatchKey = 0x1101000000000003ull;

    struct GroundRuntime {
        Object3DBase *object = nullptr;
        float centerZ = 0.0f;
        float length = 0.0f;
    };

    struct DecoPlaneRuntime {
        Object3DBase *object = nullptr;
        float centerZ = 0.0f;
    };

    struct DecoBoxRuntime {
        Object3DBase *object = nullptr;
        float centerZ = 0.0f;
    };

    void CreateDecorationPools(SceneContext *ctx) {
        if (!ctx || !defaultVars_) return;

        const float startFrontZ = spawnGroundCenterZ_ - spawnGroundDepth_ * 0.5f;
        nextDecoPlaneZ_ = startFrontZ - decoPlaneSpacingZ_;
        nextDecoBoxZ_ = startFrontZ - decoBoxMinGapZ_;

        decoPlanes_.clear();
        decoPlanes_.reserve(static_cast<std::size_t>(decoPlaneCount_));
        for (int i = 0; i < decoPlaneCount_; ++i) {
            auto obj = std::make_unique<Plane3D>();
            obj->SetName("DecorationPlane");
            obj->SetBatchKey(kDecoPlaneBatchKey, RenderType::Instancing);

            if (auto *mat = obj->GetComponent3D<Material3D>()) {
                mat->SetEnableLighting(false);
                mat->SetSampler(SamplerManager::GetSampler(DefaultSampler::LinearWrap));
                mat->SetTexture(TextureManager::GetTextureFromFileName("hexagon_alpha.png"));
            }

            obj->RegisterComponent<AlwaysRotate>(Vector3{0.0f, 0.0f, decoPlaneRotateSpeed_});

            if (defaultVars_->GetScreenBuffer3D()) {
                obj->AttachToRenderer(defaultVars_->GetScreenBuffer3D(), "Object3D.Solid.BlendNormal");
            }

            Object3DBase *ptr = obj.get();
            if (!ctx->AddObject3D(std::move(obj)) || !ptr) continue;

            DecoPlaneRuntime runtime{};
            runtime.object = ptr;
            RespawnDecoPlane(runtime);
            decoPlanes_.push_back(runtime);
        }

        decoBoxes_.clear();
        decoBoxes_.reserve(static_cast<std::size_t>(decoBoxCount_));
        for (int i = 0; i < decoBoxCount_; ++i) {
            auto obj = std::make_unique<Box>();
            obj->SetName("DecorationBox");
            obj->SetBatchKey(kDecoBoxBatchKey, RenderType::Instancing);

            if (auto *mat = obj->GetComponent3D<Material3D>()) {
                mat->SetEnableLighting(false);
                mat->SetSampler(SamplerManager::GetSampler(DefaultSampler::LinearWrap));
                mat->SetTexture(TextureManager::GetTextureFromFileName("square_alpha.png"));
            }

            const Vector3 axis = GetRandomRotateAxis();
            const float speed = GetRandomValue(decoBoxRotateSpeedMin_, decoBoxRotateSpeedMax_);
            obj->RegisterComponent<AlwaysRotate>(axis * speed);

            if (defaultVars_->GetScreenBuffer3D()) {
                obj->AttachToRenderer(defaultVars_->GetScreenBuffer3D(), "Object3D.Solid.BlendNormal");
            }

            Object3DBase *ptr = obj.get();
            if (!ctx->AddObject3D(std::move(obj)) || !ptr) continue;

            DecoBoxRuntime runtime{};
            runtime.object = ptr;
            RespawnDecoBox(runtime);
            decoBoxes_.push_back(runtime);
        }
    }

    Vector3 GetRandomRotateAxis() const {
        Vector3 axis{
            GetRandomValue(-1.0f, 1.0f),
            GetRandomValue(-1.0f, 1.0f),
            GetRandomValue(-1.0f, 1.0f)};
        if (axis.LengthSquared() <= 0.000001f) {
            axis = Vector3{0.0f, 0.0f, 1.0f};
        } else {
            axis = axis.Normalize();
        }
        return axis;
    }

    void RespawnDecoPlane(DecoPlaneRuntime &runtime) {
        if (!runtime.object) return;
        auto *tr = runtime.object->GetComponent3D<Transform3D>();
        if (!tr) return;

        const float z = nextDecoPlaneZ_;
        nextDecoPlaneZ_ -= decoPlaneSpacingZ_;

        const float scale = maxRingRadius_ * 8.0f;
        tr->SetTranslate(Vector3{0.0f, 0.0f, z});
        tr->SetRotate(Vector3{0.0f, kPi, 0.0f});
        tr->SetScale(Vector3{scale, scale, 1.0f});

        runtime.centerZ = z;
    }

    void RespawnDecoBox(DecoBoxRuntime &runtime) {
        if (!runtime.object) return;
        auto *tr = runtime.object->GetComponent3D<Transform3D>();
        if (!tr) return;

        const float z = nextDecoBoxZ_;
        nextDecoBoxZ_ -= GetRandomValue(decoBoxMinGapZ_, decoBoxMaxGapZ_);

        const float radius = GetRandomValue(maxRingRadius_ * 2.0f, maxRingRadius_ * 4.0f);
        const float angle = GetRandomValue(0.0f, kTwoPi);
        const float x = std::cos(angle) * radius;
        const float y = std::sin(angle) * radius;

        tr->SetTranslate(Vector3{x, y, z});
        tr->SetRotate(Vector3{
            GetRandomValue(0.0f, kTwoPi),
            GetRandomValue(0.0f, kTwoPi),
            GetRandomValue(0.0f, kTwoPi)});
        tr->SetScale(Vector3{decoBoxScale_, decoBoxScale_, decoBoxScale_});

        runtime.centerZ = z;
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

        auto *groundDefined = runtime.object->GetComponent3D<GroundDefined>();
        groundDefined->ResetTouchColorAnimation();

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

    //==================================================
    // スポーン時の地面の配置に関するパラメータ
    //==================================================

    float spawnGroundCenterX_ = 0.0f;
    float spawnGroundCenterY_ = -2.0f;
    float spawnGroundCenterZ_ = -2.0f;
    float spawnGroundWidth_ = 16.0f;
    float spawnGroundDepth_ = 128.0f;

    //==================================================
    // 地面オブジェクトの配置に関するパラメータ
    //==================================================

    float minRingRadius_ = 8.0f;
    float maxRingRadius_ = 64.0f;
    float minPanelWidth_ = 4.0f;
    float maxPanelWidth_ = 8.0f;
    float minPanelLength_ = 16.0f;
    float maxPanelLength_ = 64.0f;

    int minPanelsPerSegment_ = 4;
    int maxPanelsPerSegment_ = 8;

    //==================================================
    // 装飾オブジェクトの配置に関するパラメータ
    //==================================================

    int decoPlaneCount_ = 8;
    float decoPlaneSpacingZ_ = 512.0f;
    float decoPlaneRotateSpeed_ = 0.5f;
    float decoPlaneNearFadeDistance_ = 128.0f;
    float decoPlaneFarFadeDistance_ = 2048.0f;

    int decoBoxCount_ = 256;
    float decoBoxScale_ = 8.0f;
    float decoBoxRotateSpeedMin_ = 0.25f;
    float decoBoxRotateSpeedMax_ = 1.0f;
    float decoBoxMinGapZ_ = 16.0f;
    float decoBoxMaxGapZ_ = 64.0f;
    float decoBoxNearFadeDistance_ = 128.0f;
    float decoBoxFarFadeDistance_ = 2048.0f;

    //==================================================
    // 内部状態
    //==================================================

    float nextSpawnZ_ = 0.0f;
    float nextDecoPlaneZ_ = 0.0f;
    float nextDecoBoxZ_ = 0.0f;
    float currentSegmentCenterZ_ = 0.0f;
    float currentSegmentLength_ = 0.0f;
    int remainingPanelsInCurrentSegment_ = 0;

    SceneDefaultVariables *defaultVars_ = nullptr;
    Collider *collider_ = nullptr;
    Object3DBase *player_ = nullptr;
    std::vector<GroundRuntime> grounds_{};
    std::vector<DecoPlaneRuntime> decoPlanes_{};
    std::vector<DecoBoxRuntime> decoBoxes_{};
};

} // namespace KashipanEngine
