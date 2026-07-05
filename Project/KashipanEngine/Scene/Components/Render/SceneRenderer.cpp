#include "SceneRenderer.h"

#include <algorithm>

#include "Graphics/IRenderTarget.h"
#include "Graphics/PipelineManager.h"
#include "Objects/Components/Render/MeshRenderer.h"
#include "Objects/Components/Render/CameraRenderer.h"
#include "Objects/Components/Render/LightRenderer.h"
#include "Objects/Components/Render/NormalWindowObject.h"
#include "Objects/Components/Render/OverlayWindowObject.h"
#include "Objects/Components/Render/ScreenBufferObject.h"
#include "Objects/Components/Render/ShadowMapObject.h"

namespace KashipanEngine {

namespace {

/// @brief 描画先の種類ごとの描画順（オフスクリーンを先に描画する）
int GetRenderTargetKindOrder(RenderTargetKind kind) {
    switch (kind) {
    case RenderTargetKind::ShadowMapBuffer: return 0;
    case RenderTargetKind::ScreenBuffer:    return 1;
    case RenderTargetKind::Window:          return 2;
    default:                                return 3;
    }
}

} // namespace

void SceneRenderer::CollectRenderTargets(EmptyObject *targetObject, std::vector<IRenderTarget *> &out) {
    out.clear();
    if (!targetObject) return;

    for (auto *component : targetObject->GetComponents<ShadowMapObject>()) {
        if (auto *buffer = component->GetShadowMapBuffer()) out.push_back(buffer);
    }
    for (auto *component : targetObject->GetComponents<ScreenBufferObject>()) {
        if (auto *buffer = component->GetScreenBuffer()) out.push_back(buffer);
    }
    for (auto *component : targetObject->GetComponents<NormalWindowObject>()) {
        if (auto *window = component->GetWindow()) out.push_back(window);
    }
    for (auto *component : targetObject->GetComponents<OverlayWindowObject>()) {
        if (auto *window = component->GetWindow()) out.push_back(window);
    }
}

void SceneRenderer::RegisterMeshRenderer(MeshRenderer *renderer) {
    if (!renderer) return;
    if (std::find(meshRenderers_.begin(), meshRenderers_.end(), renderer) != meshRenderers_.end()) return;
    meshRenderers_.push_back(renderer);
}

void SceneRenderer::UnregisterMeshRenderer(const MeshRenderer *renderer) {
    auto it = std::find(meshRenderers_.begin(), meshRenderers_.end(), renderer);
    if (it != meshRenderers_.end()) meshRenderers_.erase(it);
}

void SceneRenderer::RegisterCameraRenderer(CameraRenderer *renderer) {
    if (!renderer) return;
    if (std::find(cameraRenderers_.begin(), cameraRenderers_.end(), renderer) != cameraRenderers_.end()) return;
    cameraRenderers_.push_back(renderer);
}

void SceneRenderer::UnregisterCameraRenderer(const CameraRenderer *renderer) {
    auto it = std::find(cameraRenderers_.begin(), cameraRenderers_.end(), renderer);
    if (it != cameraRenderers_.end()) cameraRenderers_.erase(it);
}

void SceneRenderer::RegisterLightRenderer(LightRenderer *renderer) {
    if (!renderer) return;
    if (std::find(lightRenderers_.begin(), lightRenderers_.end(), renderer) != lightRenderers_.end()) return;
    lightRenderers_.push_back(renderer);
}

void SceneRenderer::UnregisterLightRenderer(const LightRenderer *renderer) {
    auto it = std::find(lightRenderers_.begin(), lightRenderers_.end(), renderer);
    if (it != lightRenderers_.end()) lightRenderers_.erase(it);
}

const std::vector<SceneRenderer::DrawEntry> &SceneRenderer::BuildSortedDrawList(Passkey<Renderer>, PipelineManager *pipelineManager) {
    sortedDrawList_.clear();
    targetOwners_.clear();
    if (!pipelineManager) return sortedDrawList_;

    // ソートキー計算用の中間データ
    struct SortableEntry {
        DrawEntry entry;
        int kindOrder = 0;
        std::int32_t pipelinePriority = 0;
        std::uint32_t meshHandle = 0;
        std::uint32_t materialHandle = 0;
    };
    std::vector<SortableEntry> sortableEntries;
    sortableEntries.reserve(meshRenderers_.size());

    std::vector<IRenderTarget *> targets;
    for (auto *meshRenderer : meshRenderers_) {
        if (!meshRenderer) continue;
        if (!meshRenderer->IsActive()) continue;
        if (meshRenderer->GetMeshHandle() == ModelManager::kInvalidHandle) continue;

        const std::string &pipelineName = meshRenderer->GetPipelineName();
        if (pipelineName.empty() || !pipelineManager->HasPipeline(pipelineName)) continue;
        const std::int32_t pipelinePriority = pipelineManager->GetPipeline(pipelineName).RenderPriority();

        auto *targetObject = meshRenderer->GetTargetObject();
        CollectRenderTargets(targetObject, targets);
        for (auto *target : targets) {
            if (!target || !target->IsRenderTargetAvailable()) continue;
            targetOwners_[target] = targetObject;
            SortableEntry sortable;
            sortable.entry.target = target;
            sortable.entry.renderer = meshRenderer;
            sortable.kindOrder = GetRenderTargetKindOrder(target->GetRenderTargetKind());
            sortable.pipelinePriority = pipelinePriority;
            sortable.meshHandle = meshRenderer->GetMeshHandle();
            sortable.materialHandle = meshRenderer->GetMaterialHandle();
            sortableEntries.push_back(sortable);
        }
    }

    // 描画先→パイプライン優先度→パイプライン名→メッシュ→マテリアルの順でソート
    std::stable_sort(sortableEntries.begin(), sortableEntries.end(),
        [](const SortableEntry &a, const SortableEntry &b) {
            if (a.kindOrder != b.kindOrder) return a.kindOrder < b.kindOrder;
            if (a.entry.target != b.entry.target) return a.entry.target < b.entry.target;
            if (a.pipelinePriority != b.pipelinePriority) return a.pipelinePriority < b.pipelinePriority;
            const std::string &aPipeline = a.entry.renderer->GetPipelineName();
            const std::string &bPipeline = b.entry.renderer->GetPipelineName();
            if (aPipeline != bPipeline) return aPipeline < bPipeline;
            if (a.meshHandle != b.meshHandle) return a.meshHandle < b.meshHandle;
            return a.materialHandle < b.materialHandle;
        });

    sortedDrawList_.reserve(sortableEntries.size());
    for (const auto &sortable : sortableEntries) {
        sortedDrawList_.push_back(sortable.entry);
    }
    return sortedDrawList_;
}

#if defined(USE_IMGUI)
void SceneRenderer::ShowImGui() {
    ImGui::Text("MeshRenderers: %d", static_cast<int>(meshRenderers_.size()));
    ImGui::Text("CameraRenderers: %d", static_cast<int>(cameraRenderers_.size()));
    ImGui::Text("LightRenderers: %d", static_cast<int>(lightRenderers_.size()));
}
#endif

} // namespace KashipanEngine
