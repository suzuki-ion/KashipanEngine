#include "SceneRenderer.h"

#include <algorithm>

#include "Graphics/IRenderTarget.h"
#include "Graphics/PipelineManager.h"
#include "Objects/EmptyObject.h"
#include "Objects/Components/Render/MeshRenderer.h"
#include "Objects/Components/Render/SpriteRenderer.h"
#include "Objects/Components/Render/SkinnedMeshRenderer.h"
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

/// @brief ソートキー計算用の中間データ
struct SortableEntry {
    SceneRenderer::DrawEntry entry;
    int kindOrder = 0;
    std::int32_t pipelinePriority = 0;
};

/// @brief MeshRenderer/SpriteRenderer いずれの一覧からも同じ手順でDrawEntryを収集する
/// @details 両コンポーネントは GetPipelineName/GetMeshHandle/GetMaterialHandle/GetWorldMatrix/
///          GetTargetObject/IsRenderTargetIncluded という同じ形の公開APIを持つため、
///          共通の基底クラスを介さずテンプレートで共有する。
template <typename RendererT>
void CollectSortableEntries(const std::vector<RendererT *> &renderers,
    PipelineManager *pipelineManager,
    IRenderTarget *editorTarget,
    std::vector<SortableEntry> &sortableEntries,
    std::unordered_map<const IRenderTarget *, EmptyObject *> &targetOwners) {
    std::vector<IRenderTarget *> targets;
    for (auto *renderer : renderers) {
        if (!renderer) continue;
        if (!renderer->IsActive()) continue;
        if (renderer->GetMeshHandle() == ModelManager::kInvalidHandle) continue;

        const std::string &pipelineName = renderer->GetPipelineName();
        if (pipelineName.empty() || !pipelineManager->HasPipeline(pipelineName)) continue;
        const std::int32_t pipelinePriority = pipelineManager->GetPipeline(pipelineName).RenderPriority();

        // EditorOnlyオブジェクト（祖先を含む）はエディター用描画先にのみ描画する
        const EmptyObject *ownerObject = renderer->GetOwnerObject();
        const bool editorOnly = ownerObject && ownerObject->IsEditorOnlyInHierarchy();

        auto *targetObject = renderer->GetTargetObject();
        SceneRenderer::CollectRenderTargets(targetObject, targets);
        // エディター用描画先には全てのMeshRenderer/SpriteRendererを描画する
        if (editorTarget && editorTarget->IsRenderTargetAvailable()) {
            targets.push_back(editorTarget);
        }
        for (auto *target : targets) {
            if (!target || !target->IsRenderTargetAvailable()) continue;
            if (editorOnly && target != editorTarget) continue;
            // エディター用描画先には除外設定に関わらず常に描画する
            if (target != editorTarget && !renderer->IsRenderTargetIncluded(target)) continue;
            if (target != editorTarget) {
                targetOwners[target] = targetObject;
            }
            SortableEntry sortable;
            sortable.entry.target = target;
            sortable.entry.pipelineName = pipelineName;
            sortable.entry.meshHandle = renderer->GetMeshHandle();
            sortable.entry.materialHandle = renderer->GetMaterialHandle();
            sortable.entry.worldMatrix = renderer->GetWorldMatrix();
            sortable.kindOrder = GetRenderTargetKindOrder(target->GetRenderTargetKind());
            sortable.pipelinePriority = pipelinePriority;
            sortableEntries.push_back(sortable);
        }
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

void SceneRenderer::RegisterSpriteRenderer(SpriteRenderer *renderer) {
    if (!renderer) return;
    if (std::find(spriteRenderers_.begin(), spriteRenderers_.end(), renderer) != spriteRenderers_.end()) return;
    spriteRenderers_.push_back(renderer);
}

void SceneRenderer::UnregisterSpriteRenderer(const SpriteRenderer *renderer) {
    auto it = std::find(spriteRenderers_.begin(), spriteRenderers_.end(), renderer);
    if (it != spriteRenderers_.end()) spriteRenderers_.erase(it);
}

void SceneRenderer::RegisterSkinnedMeshRenderer(SkinnedMeshRenderer *renderer) {
    if (!renderer) return;
    if (std::find(skinnedMeshRenderers_.begin(), skinnedMeshRenderers_.end(), renderer) != skinnedMeshRenderers_.end()) return;
    skinnedMeshRenderers_.push_back(renderer);
}

void SceneRenderer::UnregisterSkinnedMeshRenderer(const SkinnedMeshRenderer *renderer) {
    auto it = std::find(skinnedMeshRenderers_.begin(), skinnedMeshRenderers_.end(), renderer);
    if (it != skinnedMeshRenderers_.end()) skinnedMeshRenderers_.erase(it);
}

void SceneRenderer::ResetAllSkinnedMeshRendererPoses() {
    for (auto *renderer : skinnedMeshRenderers_) {
        if (renderer) renderer->ResetAnimationToBindPose();
    }
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

    std::vector<SortableEntry> sortableEntries;
    sortableEntries.reserve(meshRenderers_.size() + spriteRenderers_.size() + skinnedMeshRenderers_.size());
    CollectSortableEntries(meshRenderers_, pipelineManager, editorTarget_, sortableEntries, targetOwners_);
    CollectSortableEntries(spriteRenderers_, pipelineManager, editorTarget_, sortableEntries, targetOwners_);

    // SkinnedMeshRendererはGPUスキニング結果バッファ(skinnedVertexBuffer)を追加で持つため、
    // MeshRenderer/SpriteRendererと形が異なりCollectSortableEntriesは使わず個別に収集する
    {
        std::vector<IRenderTarget *> targets;
        for (auto *renderer : skinnedMeshRenderers_) {
            if (!renderer || !renderer->IsActive()) continue;
            if (renderer->GetMeshHandle() == ModelManager::kInvalidHandle) continue;
            if (!renderer->HasValidSkinningData()) continue;

            const std::string &pipelineName = renderer->GetPipelineName();
            if (pipelineName.empty() || !pipelineManager->HasPipeline(pipelineName)) continue;
            const std::int32_t pipelinePriority = pipelineManager->GetPipeline(pipelineName).RenderPriority();

            // EditorOnlyオブジェクト（祖先を含む）はエディター用描画先にのみ描画する
            const EmptyObject *ownerObject = renderer->GetOwnerObject();
            const bool editorOnly = ownerObject && ownerObject->IsEditorOnlyInHierarchy();

            auto *targetObject = renderer->GetTargetObject();
            SceneRenderer::CollectRenderTargets(targetObject, targets);
            if (editorTarget_ && editorTarget_->IsRenderTargetAvailable()) {
                targets.push_back(editorTarget_);
            }
            for (auto *target : targets) {
                if (!target || !target->IsRenderTargetAvailable()) continue;
                if (editorOnly && target != editorTarget_) continue;
                if (target != editorTarget_ && !renderer->IsRenderTargetIncluded(target)) continue;
                if (target != editorTarget_) {
                    targetOwners_[target] = targetObject;
                }
                SortableEntry sortable;
                sortable.entry.target = target;
                sortable.entry.pipelineName = pipelineName;
                sortable.entry.meshHandle = renderer->GetMeshHandle();
                sortable.entry.materialHandle = renderer->GetMaterialHandle();
                sortable.entry.worldMatrix = renderer->GetWorldMatrix();
                sortable.entry.skinnedVertexBuffer = renderer->GetSkinnedVertexBuffer();
                sortable.kindOrder = GetRenderTargetKindOrder(target->GetRenderTargetKind());
                sortable.pipelinePriority = pipelinePriority;
                sortableEntries.push_back(sortable);
            }
        }
    }

    // 描画先→パイプライン優先度→パイプライン名→メッシュ→マテリアルの順でソート
    std::stable_sort(sortableEntries.begin(), sortableEntries.end(),
        [](const SortableEntry &a, const SortableEntry &b) {
            if (a.kindOrder != b.kindOrder) return a.kindOrder < b.kindOrder;
            if (a.entry.target != b.entry.target) return a.entry.target < b.entry.target;
            if (a.pipelinePriority != b.pipelinePriority) return a.pipelinePriority < b.pipelinePriority;
            if (a.entry.pipelineName != b.entry.pipelineName) return a.entry.pipelineName < b.entry.pipelineName;
            if (a.entry.meshHandle != b.entry.meshHandle) return a.entry.meshHandle < b.entry.meshHandle;
            return a.entry.materialHandle < b.entry.materialHandle;
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
    ImGui::Text("SpriteRenderers: %d", static_cast<int>(spriteRenderers_.size()));
    ImGui::Text("SkinnedMeshRenderers: %d", static_cast<int>(skinnedMeshRenderers_.size()));
    ImGui::Text("CameraRenderers: %d", static_cast<int>(cameraRenderers_.size()));
    ImGui::Text("LightRenderers: %d", static_cast<int>(lightRenderers_.size()));
}
#endif

} // namespace KashipanEngine
