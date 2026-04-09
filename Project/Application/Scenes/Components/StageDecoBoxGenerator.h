#pragma once

#include <KashipanEngine.h>
#include "Objects/Components/AlwaysRotate.h"

#include <cmath>
#include <cstdint>
#include <vector>

namespace KashipanEngine {

class StageDecoBoxGenerator final : public ISceneComponent {
public:
    struct Runtime {
        Object3DBase *object = nullptr;
        float centerZ = 0.0f;
    };

    StageDecoBoxGenerator() : ISceneComponent("StageDecoBoxGenerator", 1) {}
    ~StageDecoBoxGenerator() override = default;

    void Initialize() override {
        defaultVars_ = GetOwnerContext() ? GetOwnerContext()->GetComponent<SceneDefaultVariables>() : nullptr;
        TryGenerate();
    }

    void Update() override {
        if (!generated_ || runtimes_.empty()) return;

        auto *ctx = GetOwnerContext();
        if (!ctx) return;

        if (!player_) {
            player_ = ctx->GetObject3D("Player");
        }
        if (!player_) return;

        auto *playerTr = player_->GetComponent3D<Transform3D>();
        if (!playerTr) return;

        const float playerZ = playerTr->GetTranslate().z;
        for (auto &b : runtimes_) {
            if (!b.object) continue;
            if (b.centerZ > playerZ + recycleBehindDistance_) {
                Respawn(b);
            }
        }
    }

    void RequestGenerate() {
        requested_ = true;
        TryGenerate();
    }

    const std::vector<Runtime> &GetRuntimes() const { return runtimes_; }
    float GetNearFadeDistance() const { return nearFadeDistance_; }
    float GetFarFadeDistance() const { return farFadeDistance_; }

private:
    static constexpr float kTwoPi = 3.14159265358979323846f * 2.0f;
    static constexpr std::uint64_t kDecoBoxBatchKey = 0x1101000000000003ull;

    void TryGenerate() {
        if (generated_ || !requested_) return;

        auto *ctx = GetOwnerContext();
        if (!ctx || !defaultVars_) return;

        const float startFrontZ = spawnGroundCenterZ_ - spawnGroundDepth_ * 0.5f;
        nextSpawnZ_ = startFrontZ - minGapZ_;

        runtimes_.clear();
        runtimes_.reserve(static_cast<std::size_t>(count_));

        for (int i = 0; i < count_; ++i) {
            auto obj = std::make_unique<Box>();
            obj->SetName("DecorationBox");
            obj->SetBatchKey(kDecoBoxBatchKey, RenderType::Instancing);

            if (auto *mat = obj->GetComponent3D<Material3D>()) {
                mat->SetEnableLighting(false);
                mat->SetSampler(SamplerManager::GetSampler(DefaultSampler::LinearWrap));
                mat->SetTexture(TextureManager::GetTextureFromFileName("square_alpha.png"));
            }

            const Vector3 axis = GetRandomRotateAxis();
            const float speed = GetRandomValue(rotateSpeedMin_, rotateSpeedMax_);
            obj->RegisterComponent<AlwaysRotate>(axis * speed);

            if (defaultVars_->GetScreenBuffer3D()) {
                obj->AttachToRenderer(defaultVars_->GetScreenBuffer3D(), "Object3D.Solid.BlendNormal");
            }

            Object3DBase *ptr = obj.get();
            if (!ctx->AddObject3D(std::move(obj)) || !ptr) continue;

            Runtime runtime{};
            runtime.object = ptr;
            Respawn(runtime);
            runtimes_.push_back(runtime);
        }

        generated_ = true;
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

    void Respawn(Runtime &runtime) {
        if (!runtime.object) return;
        auto *tr = runtime.object->GetComponent3D<Transform3D>();
        if (!tr) return;

        const float z = nextSpawnZ_;
        nextSpawnZ_ -= GetRandomValue(minGapZ_, maxGapZ_);

        const float radius = GetRandomValue(maxRingRadius_ * 2.0f, maxRingRadius_ * 4.0f);
        const float angle = GetRandomValue(0.0f, kTwoPi);
        const float x = std::cos(angle) * radius;
        const float y = std::sin(angle) * radius;

        tr->SetTranslate(Vector3{x, y, z});
        tr->SetRotate(Vector3{
            GetRandomValue(0.0f, kTwoPi),
            GetRandomValue(0.0f, kTwoPi),
            GetRandomValue(0.0f, kTwoPi)});
        tr->SetScale(Vector3{scale_, scale_, scale_});

        runtime.centerZ = z;
    }

    bool requested_ = false;
    bool generated_ = false;

    int count_ = 128;
    float scale_ = 16.0f;
    float rotateSpeedMin_ = 0.25f;
    float rotateSpeedMax_ = 1.0f;
    float minGapZ_ = 16.0f;
    float maxGapZ_ = 64.0f;
    float nearFadeDistance_ = 128.0f;
    float farFadeDistance_ = 2048.0f;
    float recycleBehindDistance_ = 512.0f;

    float maxRingRadius_ = 64.0f;
    float spawnGroundCenterZ_ = -2.0f;
    float spawnGroundDepth_ = 128.0f;

    float nextSpawnZ_ = 0.0f;

    SceneDefaultVariables *defaultVars_ = nullptr;
    Object3DBase *player_ = nullptr;
    std::vector<Runtime> runtimes_{};
};

} // namespace KashipanEngine
