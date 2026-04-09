#pragma once

#include <KashipanEngine.h>

#include <cstdint>
#include <vector>

namespace KashipanEngine {

class StageNoiseWallController final : public ISceneComponent {
public:
    StageNoiseWallController() : ISceneComponent("StageNoiseWallController", 1) {}
    ~StageNoiseWallController() override = default;

    void Initialize() override {
        auto *ctx = GetOwnerContext();
        if (!ctx) return;

        defaultVars_ = ctx->GetComponent<SceneDefaultVariables>();
        if (!defaultVars_) return;

        auto root = std::make_unique<Box>();
        root->SetName("NoiseWallRoot");
        root->SetUniqueBatchKey();
        if (auto *mat = root->GetComponent3D<Material3D>()) {
            mat->SetEnableLighting(false);
            mat->SetColor(Vector4{1.0f, 1.0f, 1.0f, 0.0f});
        }
        if (auto *tr = root->GetComponent3D<Transform3D>()) {
            tr->SetTranslate(initialWallPosition_);
        }

        rootObject_ = root.get();
        (void)ctx->AddObject3D(std::move(root));

        auto *rootTr = rootObject_ ? rootObject_->GetComponent3D<Transform3D>() : nullptr;
        if (!rootTr) return;

        noiseBoxes_.clear();
        noiseBoxes_.reserve(static_cast<std::size_t>(boxCount_));

        for (int i = 0; i < boxCount_; ++i) {
            auto box = std::make_unique<Box>();
            box->SetName("NoiseWallBox");
            box->SetBatchKey(kNoiseWallBatchKey, RenderType::Instancing);

            if (defaultVars_->GetScreenBuffer3D()) {
                box->AttachToRenderer(defaultVars_->GetScreenBuffer3D(), "Object3D.Solid.BlendNormal");
            }

            if (auto *mat = box->GetComponent3D<Material3D>()) {
                mat->SetEnableLighting(false);
                mat->SetTexture(TextureManager::GetTextureFromFileName("square.png"));
                mat->SetColor(Vector4{1.0f, 0.0f, 0.0f, 1.0f});
            }

            Object3DBase *ptr = box.get();
            if (!ctx->AddObject3D(std::move(box)) || !ptr) continue;

            if (auto *tr = ptr->GetComponent3D<Transform3D>()) {
                tr->SetParentTransform(rootTr);
                RespawnNoiseBox(ptr);
            }

            noiseBoxes_.push_back(ptr);
        }
    }

    void Update() override {
        if (!rootObject_) return;

        const float dt = std::max(0.0f, GetDeltaTime() * GetGameSpeed());
        if (auto *rootTr = rootObject_->GetComponent3D<Transform3D>()) {
            Vector3 pos = rootTr->GetTranslate();
            pos.z -= moveSpeedZ_ * dt;
            rootTr->SetTranslate(pos);
        }

        spawnTimer_ += dt;
        if (spawnTimer_ >= respawnInterval_) {
            spawnTimer_ = 0.0f;
            if (!noiseBoxes_.empty()) {
                const int index = GetRandomValue(0, static_cast<int>(noiseBoxes_.size()) - 1);
                RespawnNoiseBox(noiseBoxes_[static_cast<std::size_t>(index)]);
            }
        }
    }

    float GetWallPositionZ() const {
        if (!rootObject_) return initialWallPosition_.z;
        if (auto *tr = rootObject_->GetComponent3D<Transform3D>()) {
            return tr->GetTranslate().z;
        }
        return initialWallPosition_.z;
    }

private:
    void RespawnNoiseBox(Object3DBase *box) {
        if (!box) return;
        auto *tr = box->GetComponent3D<Transform3D>();
        if (!tr) return;

        const float x = GetRandomValue(-spawnRangeX_, spawnRangeX_);
        const float y = GetRandomValue(-spawnRangeY_, spawnRangeY_);
        const float z = GetRandomValue(-spawnDepth_ * 0.5f, spawnDepth_ * 0.5f);

        const float sx = GetRandomValue(minScale_, maxScale_);
        const float sy = GetRandomValue(minScale_, maxScale_);
        const float sz = GetRandomValue(minScale_, maxScale_);

        tr->SetTranslate(Vector3{x, y, z});
        tr->SetRotate(Vector3{0.0f, 0.0f, 0.0f});
        tr->SetScale(Vector3{sx, sy, sz});
    }

    static constexpr std::uint64_t kNoiseWallBatchKey = 0x1101000000000010ull;

    SceneDefaultVariables *defaultVars_ = nullptr;
    Object3DBase *rootObject_ = nullptr;
    std::vector<Object3DBase *> noiseBoxes_{};

    int boxCount_ = 160;
    float moveSpeedZ_ = 32.0f;
    float respawnInterval_ = 0.03f;
    float spawnTimer_ = 0.0f;

    float spawnRangeX_ = 96.0f;
    float spawnRangeY_ = 96.0f;
    float spawnDepth_ = 24.0f;
    float minScale_ = 1.0f;
    float maxScale_ = 4.0f;

    Vector3 initialWallPosition_{0.0f, 0.0f, 64.0f};
};

} // namespace KashipanEngine
