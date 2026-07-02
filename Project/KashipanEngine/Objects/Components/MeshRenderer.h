#pragma once
#include "Objects/ObjectComponentHeader.h"
#include "Graphics/IRenderTarget.h"
#include "Objects/Components/MeshFilter.h"
#include "Objects/Components/Transform.h"
#include "Scene/Components/SceneRenderer.h"

namespace KashipanEngine {

class MeshRenderer final : public IObjectComponent {
public:
    OBJECT_COMPONENT_CONSTRUCTOR(MeshRenderer, 1, SetUpdatePriority(900);)
    ~MeshRenderer() override = default;

    std::unique_ptr<IObjectComponent> Clone() const override {
        auto ptr = std::make_unique<MeshRenderer>();
        ptr->targetKind_ = targetKind_;
        ptr->targetName_ = targetName_;
        ptr->pipelineName_ = pipelineName_;
        ptr->materialName_ = materialName_;
        return ptr;
    }

    void SetTarget(RenderTargetKind kind, const std::string &name) {
        targetKind_ = kind;
        targetName_ = name;
    }
    void SetTarget(const IRenderTarget *target) {
        if (!target) return;
        SetTarget(target->GetRenderTargetKind(), target->GetRenderTargetName());
    }
    void SetPipelineName(const std::string &pipelineName) { pipelineName_ = pipelineName; }
    void SetMaterialName(const std::string &materialName) { materialName_ = materialName; }

    RenderTargetKind GetTargetKind() const noexcept { return targetKind_; }
    const std::string &GetTargetName() const noexcept { return targetName_; }
    const std::string &GetPipelineName() const noexcept { return pipelineName_; }
    const std::string &GetMaterialName() const noexcept { return materialName_; }

protected:
    void Update() override {
        auto *sceneContext = GetOwnerSceneContext();
        auto *objectContext = GetOwnerObjectContext();
        if (!sceneContext || !objectContext) return;

        auto *sceneRenderer = sceneContext->GetComponent<SceneRenderer>();
        if (!sceneRenderer) {
            sceneRenderer = sceneContext->AddComponent<SceneRenderer>();
        }
        if (!sceneRenderer) return;

        auto *meshFilter = objectContext->GetComponent<MeshFilter>();
        if (!meshFilter || !meshFilter->HasMesh()) return;

        auto *transform = objectContext->GetComponent<Transform>();
        const Matrix4x4 world = transform ? transform->GetWorldMatrix() : Matrix4x4::Identity();

        sceneRenderer->Submit(
            this,
            SceneRenderer::TargetKey{ targetKind_, targetName_ },
            SceneRenderer::PipelineKey{ pipelineName_ },
            SceneRenderer::MeshKey{ meshFilter->GetMeshHandle() },
            SceneRenderer::MaterialKey{ materialName_ },
            world);
    }

#if defined(USE_IMGUI)
    void ShowImGui() override {
        int kind = static_cast<int>(targetKind_);
        const char *items[] = { "Window", "ScreenBuffer", "ShadowMapBuffer" };
        if (ImGui::Combo("Target", &kind, items, 3)) {
            targetKind_ = static_cast<RenderTargetKind>(kind);
        }
        ImGui::InputText("TargetName", &targetName_);
        ImGui::InputText("Pipeline", &pipelineName_);
        ImGui::InputText("Material", &materialName_);
    }
#endif

    JSON SaveToJson() const override {
        JSON json = JSON::object();
        json["targetKind"] = static_cast<int>(targetKind_);
        json["targetName"] = targetName_;
        json["pipelineName"] = pipelineName_;
        json["materialName"] = materialName_;
        return json;
    }

    bool LoadFromJson(const JSON &json) override {
        targetKind_ = static_cast<RenderTargetKind>(json.value("targetKind", static_cast<int>(RenderTargetKind::ScreenBuffer)));
        targetName_ = json.value("targetName", std::string{});
        pipelineName_ = json.value("pipelineName", std::string{ "Object3D.Solid.BlendNormal" });
        materialName_ = json.value("materialName", std::string{ "Default" });
        return true;
    }

private:
    RenderTargetKind targetKind_ = RenderTargetKind::ScreenBuffer;
    std::string targetName_;
    std::string pipelineName_ = "Object3D.Solid.BlendNormal";
    std::string materialName_ = "Default";
};

REGISTER_COMPONENT_OBJECT(MeshRenderer)

} // namespace KashipanEngine
