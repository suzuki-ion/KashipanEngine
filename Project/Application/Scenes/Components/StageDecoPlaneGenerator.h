#pragma once

#include <KashipanEngine.h>
#include "Objects/Components/AlwaysRotate.h"

#include <cstdint>
#include <vector>

namespace KashipanEngine {

class StageDecoPlaneGenerator final : public ISceneComponent {
public:
    struct Runtime {
        Object3DBase *frontObject = nullptr;
        Object3DBase *backObject = nullptr;
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
            player_ = ctx->GetObject3D("PlayerRoot");
        }
        if (!player_) return;

        auto *playerTr = player_->GetComponent3D<Transform3D>();
        if (!playerTr) return;

        const float playerZ = playerTr->GetTranslate().z;
        for (auto &p : runtimes_) {
            if (!p.frontObject && !p.backObject) continue;
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
    static constexpr float kTiltStepRad = 3.14159265358979323846f / 12.0f; // 15度
    static constexpr std::uint64_t kDecoPlaneBatchKey = 0x1101000000000002ull;

    Object3DBase *CreatePlane(SceneContext *ctx) {
        if (!ctx) return nullptr;

        auto obj = std::make_unique<Plane3D>();
        obj->SetName("DecorationPlane");
        obj->SetBatchKey(kDecoPlaneBatchKey, RenderType::Instancing);

        if (auto *mat = obj->GetComponent3D<Material3D>()) {
            mat->SetEnableLighting(false);
            mat->SetSampler(SamplerManager::GetSampler(DefaultSampler::LinearWrap));
            mat->SetTexture(TextureManager::GetTextureFromFileName("hexagon_alpha.png"));
        }

        obj->RegisterComponent<AlwaysRotate>(Vector3{0.0f, 0.0f, rotateSpeed_});

        if (defaultVars_ && defaultVars_->GetScreenBuffer3D()) {
            obj->AttachToRenderer(defaultVars_->GetScreenBuffer3D(), "Object3D.Solid.BlendNormal");
        }

        Object3DBase *ptr = obj.get();
        if (!ctx->AddObject3D(std::move(obj))) return nullptr;
        return ptr;
    }

    void TryGenerate() {
        if (generated_ || !requested_) return;

        auto *ctx = GetOwnerContext();
        if (!ctx || !defaultVars_) return;

        const float startFrontZ = spawnGroundCenterZ_ - spawnGroundDepth_ * 0.5f;
        nextSpawnZ_ = startFrontZ - spacingZ_;

        runtimes_.clear();
        runtimes_.reserve(static_cast<std::size_t>(count_));

        for (int i = 0; i < count_; ++i) {
            Object3DBase *front = CreatePlane(ctx);
            Object3DBase *back = CreatePlane(ctx);
            if (!front || !back) continue;

            Runtime runtime{};
            runtime.frontObject = front;
            runtime.backObject = back;
            Respawn(runtime);
            runtimes_.push_back(runtime);
        }

        generated_ = true;
    }

    void Respawn(Runtime &runtime) {
        if (!runtime.frontObject || !runtime.backObject) return;

        auto *frontTr = runtime.frontObject->GetComponent3D<Transform3D>();
        auto *backTr = runtime.backObject->GetComponent3D<Transform3D>();
        if (!frontTr || !backTr) return;

        const float z = nextSpawnZ_;
        nextSpawnZ_ -= spacingZ_;

        const float scale = maxRingRadius_ * 8.0f;
        const Vector3 t{0.0f, 0.0f, z};
        const Vector3 s{scale, scale, 1.0f};
        frontTr->SetTranslate(t);
        frontTr->SetRotate(Vector3{0.0f, kPi, nextTiltAngle_});
        frontTr->SetScale(s);

        backTr->SetTranslate(t);
        backTr->SetRotate(Vector3{0.0f, 0.0f, nextTiltAngle_});
        backTr->SetScale(s);

        nextTiltAngle_ += kTiltStepRad;
        if (nextTiltAngle_ > kPi * 2.0f) {
            nextTiltAngle_ -= kPi * 2.0f;
        }

        runtime.centerZ = z;
    }

    bool requested_ = false;
    bool generated_ = false;

    int count_ = 32;
    float spacingZ_ = 512.0f;
    float rotateSpeed_ = 0.5f;
    float nearFadeDistance_ = 128.0f;
    float farFadeDistance_ = 2048.0f;
    float recycleBehindDistance_ = 2048.0f;

    float maxRingRadius_ = 64.0f;
    float spawnGroundCenterZ_ = -2.0f;
    float spawnGroundDepth_ = 0.0f;

    float nextSpawnZ_ = 0.0f;
    float nextTiltAngle_ = 0.0f;

    SceneDefaultVariables *defaultVars_ = nullptr;
    Object3DBase *player_ = nullptr;
    std::vector<Runtime> runtimes_{};
};

} // namespace KashipanEngine
