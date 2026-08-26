#pragma once
#include <cstdint>
#include <map>
#include <string>
#include <vector>

#include "Assets/ModelManager.h"
#include "Graphics/IRenderTarget.h"
#include "Graphics/MaterialManager.h"
#include "Objects/Components/Transform.h"
#include "Scene/Components/SceneComponentHeader.h"

namespace KashipanEngine {

class MeshRenderer;

class SceneRenderer final : public ISceneComponent {
public:
    struct MaterialKey {
        std::string name;
        MaterialManager::MaterialHandle handle = MaterialManager::kInvalidHandle;
        bool operator<(const MaterialKey &other) const noexcept {
            if (handle != other.handle) return handle < other.handle;
            return name < other.name;
        }
    };
    struct MeshKey {
        ModelManager::ModelHandle handle = ModelManager::kInvalidHandle;
        bool operator<(const MeshKey &other) const noexcept { return handle < other.handle; }
    };
    struct PipelineKey {
        std::string name;
        bool operator<(const PipelineKey &other) const noexcept { return name < other.name; }
    };
    struct TargetKey {
        RenderTargetKind kind = RenderTargetKind::ScreenBuffer;
        std::string name;
        bool operator<(const TargetKey &other) const noexcept {
            if (kind != other.kind) return static_cast<int>(kind) < static_cast<int>(other.kind);
            return name < other.name;
        }
    };
    struct Instance {
        const MeshRenderer *renderer = nullptr;
        Matrix4x4 worldMatrix = Matrix4x4::Identity();
    };

    using MaterialBuckets = std::map<MaterialKey, std::vector<Instance>>;
    using MeshBuckets = std::map<MeshKey, MaterialBuckets>;
    using PipelineBuckets = std::map<PipelineKey, MeshBuckets>;
    using TargetBuckets = std::map<TargetKey, PipelineBuckets>;

    SCENE_COMPONENT_CONSTRUCTOR(SceneRenderer, 1, SetUpdatePriority(1000);)
    ~SceneRenderer() override = default;

    std::unique_ptr<ISceneComponent> Clone() const override {
        return std::make_unique<SceneRenderer>();
    }

    void Submit(
        const MeshRenderer *renderer,
        const TargetKey &target,
        const PipelineKey &pipeline,
        const MeshKey &mesh,
        const MaterialKey &material,
        const Matrix4x4 &worldMatrix) {
        if (!renderer) return;
        frameBatches_[target][pipeline][mesh][material].push_back({ renderer, worldMatrix });
    }

    const TargetBuckets &GetFrameBatches() const noexcept { return frameBatches_; }
    void ClearFrameBatches() { frameBatches_.clear(); }

protected:
    void Update() override {
        ClearFrameBatches();
    }

#if defined(USE_IMGUI)
    void ShowImGui() override {
        ImGui::Text("Targets: %d", static_cast<int>(frameBatches_.size()));
        for (const auto &[target, pipelines] : frameBatches_) {
            ImGui::BulletText("%s / %s: %d pipeline(s)",
                target.kind == RenderTargetKind::Window ? "Window" :
                target.kind == RenderTargetKind::ShadowMapBuffer ? "ShadowMapBuffer" : "ScreenBuffer",
                target.name.c_str(),
                static_cast<int>(pipelines.size()));
        }
    }
#endif

private:
    TargetBuckets frameBatches_;
};

REGISTER_COMPONENT_SCENE(SceneRenderer)

} // namespace KashipanEngine
