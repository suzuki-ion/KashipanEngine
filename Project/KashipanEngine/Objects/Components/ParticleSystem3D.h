#pragma once
#include "Objects/Components/ParticleSystemBase.h"
#include "Objects/Components/MeshFilter.h"
#include "Objects/Components/Render/Camera3D.h"
#include "Objects/Components/Render/MeshRenderer.h"
#include "Assets/ModelManager.h"

namespace KashipanEngine {

/// @brief 3D用パーティクルシステム
/// @details 付与されたオブジェクトの子オブジェクトとして、MeshFilter + MeshRenderer を
///          持つパーティクル用オブジェクトを一定間隔で生成する。共通のロジックは
///          ParticleSystemBase を参照。既定のメッシュは "PrimitiveMesh-UVSphere"。
class ParticleSystem3D final : public ParticleSystemBase {
public:
    ParticleSystem3D() : ParticleSystemBase("ParticleSystem3D", 0xFF, GetComponentTypeID<ParticleSystem3D>()) {
        if (meshAssetPath_.empty()) meshAssetPath_ = "PrimitiveMesh-UVSphere";
        if (pipelineName_.empty()) pipelineName_ = "Object3D.Solid.BlendNormal";
    }
    ~ParticleSystem3D() override = default;

    std::unique_ptr<IObjectComponent> Clone() const override {
        auto ptr = std::make_unique<ParticleSystem3D>();
        ptr->CopyBaseFieldsFrom(*this);
        return ptr;
    }

protected:
    void Initialize() override { InitializeBase(); }
    void Finalize() override { FinalizeBase(); }

    void Update() override {
        UpdateParticles([this](EmptyObject *particle) {
            if (auto *filter = particle->AddComponent<MeshFilter>()) {
                filter->SetMeshHandle(ModelManager::GetModelHandleFromAssetPath(meshAssetPath_));
            }
            if (auto *renderer = particle->AddComponent<MeshRenderer>()) {
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

    /// @brief ビルボード化用に、シーン内で最初に見つかった Camera3D を持つオブジェクトを使う
    EmptyObject *ResolveBillboardCameraObject() const override {
        auto *sceneContext = GetOwnerSceneContext();
        if (!sceneContext) return nullptr;
        for (const auto &object : sceneContext->GetSceneObjects()) {
            if (object && object->GetComponent<Camera3D>()) return object.get();
        }
        return nullptr;
    }
};

REGISTER_COMPONENT_OBJECT(ParticleSystem3D)

} // namespace KashipanEngine
