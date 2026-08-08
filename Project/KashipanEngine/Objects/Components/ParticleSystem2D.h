#pragma once
#include "Objects/Components/ParticleSystemBase.h"
#include "Objects/Components/MeshFilter.h"
#include "Objects/Components/Render/Camera2D.h"
#include "Objects/Components/Render/SpriteRenderer.h"
#include "Assets/ModelManager.h"

namespace KashipanEngine {

/// @brief 2D用パーティクルシステム
/// @details 付与されたオブジェクトの子オブジェクトとして、MeshFilter + SpriteRenderer を
///          持つパーティクル用オブジェクトを一定間隔で生成する。共通のロジックは
///          ParticleSystemBase を参照。既定のメッシュは "PrimitiveMesh-Circle2D"。
class ParticleSystem2D final : public ParticleSystemBase {
public:
    ParticleSystem2D() : ParticleSystemBase("ParticleSystem2D", 0xFF, GetComponentTypeID<ParticleSystem2D>(), true) {
        if (meshAssetPath_.empty()) meshAssetPath_ = "PrimitiveMesh-Circle2D";
        if (pipelineName_.empty()) pipelineName_ = "Object2D.DoubleSidedCulling.BlendNormal";
    }
    ~ParticleSystem2D() override = default;

    std::unique_ptr<IObjectComponent> Clone() const override {
        auto ptr = std::make_unique<ParticleSystem2D>();
        ptr->CopyBaseFieldsFrom(*this);
        return ptr;
    }

protected:
    void Initialize() override { InitializeBase(); }
    void Finalize() override { FinalizeBase(); }

    void Update() override {
        if (IsGPUSimulation()) {
            UpdateParticlesGPU();
            return;
        }
        UpdateParticles([this](EmptyObject *particle) {
            if (auto *filter = particle->AddComponent<MeshFilter>()) {
                filter->SetMeshHandle(ModelManager::GetModelHandleFromAssetPath(meshAssetPath_));
            }
            if (auto *renderer = particle->AddComponent<SpriteRenderer>()) {
                renderer->SetTargetObject(targetObjectID_);
                renderer->SetPipelineName(pipelineName_);
                renderer->SetMaterialName(materialName_);
            }
        });
    }

#if defined(USE_IMGUI)
    void ShowImGui() override { ShowBaseFieldsImGui(); }
#endif

    JSON SaveToJson() const override { return SaveBaseFieldsJson(); }
    bool LoadFromJson(const JSON &json) override { LoadBaseFieldsJson(json); return true; }

    /// @brief ビルボード化用に、シーン内で最初に見つかった Camera2D を持つオブジェクトを使う
    EmptyObject *ResolveBillboardCameraObject() const override {
        auto *sceneContext = GetOwnerSceneContext();
        if (!sceneContext) return nullptr;
        for (auto *object : sceneContext->GetSceneObjects()) {
            if (object && object->GetComponent<Camera2D>()) return object;
        }
        return nullptr;
    }
};

REGISTER_COMPONENT_OBJECT(ParticleSystem2D)

} // namespace KashipanEngine
