#pragma once

#include <KashipanEngine.h>
#include "Objects/Components/AlwaysRotate.h"

#include <cstdint>
#include <vector>

namespace KashipanEngine {

class StageDecoPlaneGenerator final : public ISceneComponent {
public:
    struct Runtime {
        Object3DBase *object = nullptr;
        float centerZ = 0.0f;
    };

    StageDecoPlaneGenerator() : ISceneComponent("StageDecoPlaneGenerator", 1) {}
    ~StageDecoPlaneGenerator() override = default;

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
        for (auto &p : runtimes_) {
            if (!p.object) continue;
            if (p.centerZ > playerZ + recycleBehindDistance_) {
                Respawn(p);
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
    static constexpr float kPi = 3.14159265358979323846f;
    static constexpr std::uint64_t kDecoPlaneBatchKey = 0x1101000000000002ull;

    void TryGenerate() {
        if (generated_ || !requested_) return;

        auto *ctx = GetOwnerContext();
        if (!ctx || !defaultVars_) return;

        const float startFrontZ = spawnGroundCenterZ_ - spawnGroundDepth_ * 0.5f;
        nextSpawnZ_ = startFrontZ - spacingZ_;

        runtimes_.clear();
        runtimes_.reserve(static_cast<std::size_t>(count_));

        for (int i = 0; i < count_; ++i) {
            auto obj = std::make_unique<Plane3D>();
            obj->SetName("DecorationPlane");
            obj->SetBatchKey(kDecoPlaneBatchKey, RenderType::Instancing);

            if (auto *mat = obj->GetComponent3D<Material3D>()) {
                mat->SetEnableLighting(false);
                mat->SetSampler(SamplerManager::GetSampler(DefaultSampler::LinearWrap));
                mat->SetTexture(TextureManager::GetTextureFromFileName("hexagon_alpha.png"));
            }

            obj->RegisterComponent<AlwaysRotate>(Vector3{0.0f, 0.0f, rotateSpeed_});

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

    void Respawn(Runtime &runtime) {
        if (!runtime.object) return;
        auto *tr = runtime.object->GetComponent3D<Transform3D>();
        if (!tr) return;

        const float z = nextSpawnZ_;
        nextSpawnZ_ -= spacingZ_;

        const float scale = maxRingRadius_ * 8.0f;
        tr->SetTranslate(Vector3{0.0f, 0.0f, z});
        tr->SetRotate(Vector3{0.0f, kPi, 0.0f});
        tr->SetScale(Vector3{scale, scale, 1.0f});

        runtime.centerZ = z;
    }

    bool requested_ = false;
    bool generated_ = false;

    int count_ = 8;
    float spacingZ_ = 512.0f;
    float rotateSpeed_ = 0.5f;
    float nearFadeDistance_ = 128.0f;
    float farFadeDistance_ = 2048.0f;
    float recycleBehindDistance_ = 64.0f;

    float maxRingRadius_ = 64.0f;
    float spawnGroundCenterZ_ = -2.0f;
    float spawnGroundDepth_ = 128.0f;

    float nextSpawnZ_ = 0.0f;

    SceneDefaultVariables *defaultVars_ = nullptr;
    Object3DBase *player_ = nullptr;
    std::vector<Runtime> runtimes_{};
};

} // namespace KashipanEngine
