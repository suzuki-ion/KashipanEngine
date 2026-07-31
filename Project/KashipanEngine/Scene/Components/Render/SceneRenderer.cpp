#include "SceneRenderer.h"

#include <algorithm>
#include <type_traits>

#include "Graphics/IRenderTarget.h"
#include "Utilities/Translation.h"
#include "Graphics/PipelineManager.h"
#include "Objects/EmptyObject.h"
#include "Objects/Components/Shake.h"
#include "Objects/Components/PostProcessing/IPostProcessComponent.h"
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

/// @brief ソートキー計算用の中間データ（SceneRenderer::RankedDrawEntryそのもの。
///        キャッシュ側と同じ型にしておくことでマージ処理を共有できる）
using SortableEntry = SceneRenderer::RankedDrawEntry;

/// @brief レンダラーとサブメッシュ番号からマテリアルハンドルを取得する
/// @details マテリアルスロットを持たないSpriteRendererは常に単一マテリアルを返す
template <typename RendererT>
MaterialManager::MaterialHandle GetMaterialHandleForSubMesh(const RendererT *renderer, size_t subMeshIndex) {
    if constexpr (std::is_same_v<RendererT, MeshRenderer> || std::is_same_v<RendererT, SkinnedMeshRenderer>) {
        return renderer->GetMaterialHandleAt(subMeshIndex);
    } else {
        (void)subMeshIndex;
        return renderer->GetMaterialHandle();
    }
}

/// @brief インスタンスカラー（オブジェクト単位の色）を持つレンダラー（現状MeshRendererのみ）から取得する。
///        持たない型（SpriteRenderer/SkinnedMeshRenderer）は既定値（白＋Multiply＝見た目に影響しない）を返す
template <typename RendererT>
Vector4 GetInstanceColorFor(const RendererT *renderer) {
    if constexpr (std::is_same_v<RendererT, MeshRenderer>) {
        return renderer->GetInstanceColor();
    } else {
        (void)renderer;
        return Vector4(1.0f, 1.0f, 1.0f, 1.0f);
    }
}
template <typename RendererT>
int GetInstanceColorBlendModeFor(const RendererT *renderer) {
    if constexpr (std::is_same_v<RendererT, MeshRenderer>) {
        return static_cast<int>(renderer->GetInstanceColorBlendMode());
    } else {
        (void)renderer;
        return 1; // Multiply
    }
}

/// @brief MeshRenderer/SpriteRenderer いずれの一覧からも同じ手順でDrawEntryを収集する
/// @details 両コンポーネントは GetPipelineName/GetMeshHandle/GetMaterialHandle/GetWorldMatrix/
///          GetTargetObject/IsRenderTargetIncluded という同じ形の公開APIを持つため、
///          共通の基底クラスを介さずテンプレートで共有する。
/// @param onlyCustomTarget trueの場合、targetObjectID（描画先を明示指定するID）が有効なレンダラーのみを
///        対象にする。falseの場合はtargetObjectID未指定（＝エディター用描画先にのみ描画される）の
///        レンダラーのみを対象にする。両者は排他なので、毎フレーム両方呼んでも重複は発生しない
template <typename RendererT>
void CollectSortableEntries(const std::vector<RendererT *> &renderers,
    PipelineManager *pipelineManager,
    IRenderTarget *editorTarget,
    std::vector<SortableEntry> &sortableEntries,
    std::unordered_map<const IRenderTarget *, EmptyObject *> &targetOwners,
    bool onlyCustomTarget) {
    std::vector<IRenderTarget *> targets;
    for (auto *renderer : renderers) {
        if (!renderer) continue;
        if (renderer->GetTargetObjectID().IsValid() != onlyCustomTarget) continue;
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
        // サブメッシュ（マテリアルごとのインデックス範囲）ごとに1エントリ作る
        const auto &subMeshes = ModelManager::GetModelData(renderer->GetMeshHandle()).GetSubMeshes();
        const size_t subMeshCount = std::max<size_t>(1, subMeshes.size());

        for (auto *target : targets) {
            if (!target || !target->IsRenderTargetAvailable()) continue;
            if (editorOnly && target != editorTarget) continue;
            // エディター用描画先には除外設定に関わらず常に描画する
            if (target != editorTarget && !renderer->IsRenderTargetIncluded(target)) continue;
            if (target != editorTarget) {
                targetOwners[target] = targetObject;
            }
            for (size_t subMeshIndex = 0; subMeshIndex < subMeshCount; ++subMeshIndex) {
                SortableEntry sortable;
                sortable.entry.target = target;
                sortable.entry.pipelineName = pipelineName;
                sortable.entry.meshHandle = renderer->GetMeshHandle();
                sortable.entry.materialHandle = GetMaterialHandleForSubMesh(renderer, subMeshIndex);
                if (!subMeshes.empty()) {
                    sortable.entry.indexStart = subMeshes[subMeshIndex].indexStart;
                    sortable.entry.indexCount = subMeshes[subMeshIndex].indexCount;
                }
                sortable.entry.worldMatrix = Shake::ApplyRenderOnlyOffsets(
                    renderer->GetOwnerObject(), renderer->GetWorldMatrix());
                sortable.entry.instanceColor = GetInstanceColorFor(renderer);
                sortable.entry.instanceColorBlendMode = GetInstanceColorBlendModeFor(renderer);
                sortable.kindOrder = GetRenderTargetKindOrder(target->GetRenderTargetKind());
                sortable.pipelinePriority = pipelinePriority;
                sortableEntries.push_back(sortable);
            }
        }
    }
}

/// @brief 描画先→パイプライン優先度→パイプライン名→メッシュ→サブメッシュ→マテリアルの順で比較する
bool CompareSortableEntry(const SortableEntry &a, const SortableEntry &b) {
    if (a.kindOrder != b.kindOrder) return a.kindOrder < b.kindOrder;
    if (a.entry.target != b.entry.target) return a.entry.target < b.entry.target;
    if (a.pipelinePriority != b.pipelinePriority) return a.pipelinePriority < b.pipelinePriority;
    if (a.entry.pipelineName != b.entry.pipelineName) return a.entry.pipelineName < b.entry.pipelineName;
    if (a.entry.meshHandle != b.entry.meshHandle) return a.entry.meshHandle < b.entry.meshHandle;
    if (a.entry.indexStart != b.entry.indexStart) return a.entry.indexStart < b.entry.indexStart;
    if (a.entry.indexCount != b.entry.indexCount) return a.entry.indexCount < b.entry.indexCount;
    return a.entry.materialHandle < b.entry.materialHandle;
}

/// @brief キャッシュ対象（targetObjectID未指定＝エディター用描画先にのみ描画するMesh/SpriteRenderer）を収集する
/// @details targetObjectIDが有効なレンダラーは描画先コンポーネントの変化を都度確認する必要があるため対象外
///          （BuildSortedDrawList側で毎フレーム別途収集される）。アクティブ状態はここではフィルタしない
///          （非アクティブなレンダラーも一旦キャッシュしておき、毎フレームの再確認で除外することで、
///          再アクティブ化した際にも次回のダーティ再構築を待たずに描画リストへ反映されるようにするため）
template <typename RendererT>
void CollectCacheableEntries(const std::vector<RendererT *> &renderers,
    PipelineManager *pipelineManager,
    IRenderTarget *editorTarget,
    std::vector<SceneRenderer::CachedRankedEntry> &out) {
    for (auto *renderer : renderers) {
        if (!renderer) continue;
        if (renderer->GetTargetObjectID().IsValid()) continue;
        if (renderer->GetMeshHandle() == ModelManager::kInvalidHandle) continue;

        const std::string &pipelineName = renderer->GetPipelineName();
        if (pipelineName.empty() || !pipelineManager->HasPipeline(pipelineName)) continue;

        // サブメッシュ（マテリアルごとのインデックス範囲）ごとに1エントリ作る
        const auto &subMeshes = ModelManager::GetModelData(renderer->GetMeshHandle()).GetSubMeshes();
        const size_t subMeshCount = std::max<size_t>(1, subMeshes.size());
        for (size_t subMeshIndex = 0; subMeshIndex < subMeshCount; ++subMeshIndex) {
            SceneRenderer::CachedRankedEntry cached;
            cached.ranked.entry.target = editorTarget;
            cached.ranked.entry.pipelineName = pipelineName;
            cached.ranked.entry.meshHandle = renderer->GetMeshHandle();
            cached.ranked.entry.materialHandle = GetMaterialHandleForSubMesh(renderer, subMeshIndex);
            if (!subMeshes.empty()) {
                cached.ranked.entry.indexStart = subMeshes[subMeshIndex].indexStart;
                cached.ranked.entry.indexCount = subMeshes[subMeshIndex].indexCount;
            }
            cached.ranked.entry.instanceColor = GetInstanceColorFor(renderer);
            cached.ranked.entry.instanceColorBlendMode = GetInstanceColorBlendModeFor(renderer);
            cached.ranked.kindOrder = GetRenderTargetKindOrder(editorTarget->GetRenderTargetKind());
            cached.ranked.pipelinePriority = pipelineManager->GetPipeline(pipelineName).RenderPriority();
            cached.source = renderer;
            out.push_back(std::move(cached));
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
        // WindowObject は Window の所有者ではないため、フレーム末尾の CommitDestroy 後も
        // 破棄済み Window のアドレスを保持している場合がある。仮想関数を呼ぶ前に、
        // Window の管理マップにまだ登録されていることをポインター比較だけで確認する。
        if (auto *window = component->GetWindow(); Window::IsExist(window)) out.push_back(window);
    }
    for (auto *component : targetObject->GetComponents<OverlayWindowObject>()) {
        if (auto *window = component->GetWindow(); Window::IsExist(window)) out.push_back(window);
    }
}

void SceneRenderer::RegisterMeshRenderer(MeshRenderer *renderer) {
    if (!renderer) return;
    if (std::find(meshRenderers_.begin(), meshRenderers_.end(), renderer) != meshRenderers_.end()) return;
    meshRenderers_.push_back(renderer);
    drawListDirty_ = true;
}

void SceneRenderer::UnregisterMeshRenderer(const MeshRenderer *renderer) {
    auto it = std::find(meshRenderers_.begin(), meshRenderers_.end(), renderer);
    if (it != meshRenderers_.end()) {
        meshRenderers_.erase(it);
        drawListDirty_ = true;
    }
}

void SceneRenderer::RegisterSpriteRenderer(SpriteRenderer *renderer) {
    if (!renderer) return;
    if (std::find(spriteRenderers_.begin(), spriteRenderers_.end(), renderer) != spriteRenderers_.end()) return;
    spriteRenderers_.push_back(renderer);
    drawListDirty_ = true;
}

void SceneRenderer::UnregisterSpriteRenderer(const SpriteRenderer *renderer) {
    auto it = std::find(spriteRenderers_.begin(), spriteRenderers_.end(), renderer);
    if (it != spriteRenderers_.end()) {
        spriteRenderers_.erase(it);
        drawListDirty_ = true;
    }
}

void SceneRenderer::RegisterTextRenderer(TextRenderer *renderer) {
    if (!renderer) return;
    if (std::find(textRenderers_.begin(), textRenderers_.end(), renderer) != textRenderers_.end()) return;
    textRenderers_.push_back(renderer);
}

void SceneRenderer::UnregisterTextRenderer(const TextRenderer *renderer) {
    auto it = std::find(textRenderers_.begin(), textRenderers_.end(), renderer);
    if (it != textRenderers_.end()) textRenderers_.erase(it);
}

void SceneRenderer::RegisterGpuParticleEmitter(ParticleSystemBase *emitter) {
    if (!emitter) return;
    if (std::find(gpuParticleEmitters_.begin(), gpuParticleEmitters_.end(), emitter) != gpuParticleEmitters_.end()) return;
    gpuParticleEmitters_.push_back(emitter);
}

void SceneRenderer::UnregisterGpuParticleEmitter(const ParticleSystemBase *emitter) {
    auto it = std::find(gpuParticleEmitters_.begin(), gpuParticleEmitters_.end(), emitter);
    if (it != gpuParticleEmitters_.end()) gpuParticleEmitters_.erase(it);
}

void SceneRenderer::RegisterPostProcessComponent(IPostProcessComponent *component) {
    if (!component) return;
    if (std::find(postProcessComponents_.begin(), postProcessComponents_.end(), component) != postProcessComponents_.end()) return;
    postProcessComponents_.push_back(component);
}

void SceneRenderer::UnregisterPostProcessComponent(const IPostProcessComponent *component) {
    auto it = std::find(postProcessComponents_.begin(), postProcessComponents_.end(), component);
    if (it != postProcessComponents_.end()) postProcessComponents_.erase(it);
}

std::vector<IPostProcessComponent *> SceneRenderer::GetPostProcessComponentsFor(const EmptyObject *ownerObject) const {
    std::vector<IPostProcessComponent *> result;
    if (!ownerObject) return result;
    for (auto *component : postProcessComponents_) {
        if (component && component->GetOwnerObject() == ownerObject) {
            result.push_back(component);
        }
    }
    return result;
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

void SceneRenderer::RebuildCachedEntries(PipelineManager *pipelineManager) {
    cachedEntries_.clear();
    if (!pipelineManager || !editorTarget_) return;
    CollectCacheableEntries(meshRenderers_, pipelineManager, editorTarget_, cachedEntries_);
    CollectCacheableEntries(spriteRenderers_, pipelineManager, editorTarget_, cachedEntries_);
    std::stable_sort(cachedEntries_.begin(), cachedEntries_.end(),
        [](const CachedRankedEntry &a, const CachedRankedEntry &b) {
            return CompareSortableEntry(a.ranked, b.ranked);
        });
}

const std::vector<SceneRenderer::DrawEntry> &SceneRenderer::BuildSortedDrawList(Passkey<Renderer>, PipelineManager *pipelineManager) {
    sortedDrawList_.clear();
    targetOwners_.clear();
    if (!pipelineManager) return sortedDrawList_;

    if (drawListDirty_) {
        RebuildCachedEntries(pipelineManager);
        drawListDirty_ = false;
    }

    // targetObjectID指定あり（カスタム描画先を持つ）Mesh/SpriteRenderer、およびSkinnedMeshRendererは、
    // 描画先コンポーネントの変化・GPUスキニング有効性など動的な要素を都度確認する必要があるため、
    // キャッシュ対象にせず毎フレーム収集する（targetObjectID未指定＝エディター用描画先のみに描画する
    // 分はcachedEntries_側でまとめて扱うため、ここでは重複しない）
    std::vector<SortableEntry> freshEntries;
    CollectSortableEntries(meshRenderers_, pipelineManager, editorTarget_, freshEntries, targetOwners_, /*onlyCustomTarget=*/true);
    CollectSortableEntries(spriteRenderers_, pipelineManager, editorTarget_, freshEntries, targetOwners_, /*onlyCustomTarget=*/true);

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
            // サブメッシュ（マテリアルごとのインデックス範囲）ごとに1エントリ作る
            const auto &subMeshes = ModelManager::GetModelData(renderer->GetMeshHandle()).GetSubMeshes();
            const size_t subMeshCount = std::max<size_t>(1, subMeshes.size());

            for (auto *target : targets) {
                if (!target || !target->IsRenderTargetAvailable()) continue;
                if (editorOnly && target != editorTarget_) continue;
                if (target != editorTarget_ && !renderer->IsRenderTargetIncluded(target)) continue;
                if (target != editorTarget_) {
                    targetOwners_[target] = targetObject;
                }
                for (size_t subMeshIndex = 0; subMeshIndex < subMeshCount; ++subMeshIndex) {
                    SortableEntry sortable;
                    sortable.entry.target = target;
                    sortable.entry.pipelineName = pipelineName;
                    sortable.entry.meshHandle = renderer->GetMeshHandle();
                    sortable.entry.materialHandle = renderer->GetMaterialHandleAt(subMeshIndex);
                    if (!subMeshes.empty()) {
                        sortable.entry.indexStart = subMeshes[subMeshIndex].indexStart;
                        sortable.entry.indexCount = subMeshes[subMeshIndex].indexCount;
                    }
                    // スキンドメッシュにもRenderOnlyシェイクを描画行列上だけで適用する。
                    // TransformやボーンTransformは変更しないため、物理・追従処理には影響しない
                    sortable.entry.worldMatrix = Shake::ApplyRenderOnlyOffsets(
                        renderer->GetOwnerObject(), renderer->GetWorldMatrix());
                    sortable.entry.skinnedVertexBuffer = renderer->GetSkinnedVertexBuffer();
                    sortable.kindOrder = GetRenderTargetKindOrder(target->GetRenderTargetKind());
                    sortable.pipelinePriority = pipelinePriority;
                    freshEntries.push_back(sortable);
                }
            }
        }
    }

    // 描画先→パイプライン優先度→パイプライン名→メッシュ→マテリアルの順でソート
    std::stable_sort(freshEntries.begin(), freshEntries.end(), CompareSortableEntry);

    // キャッシュ分（targetObjectID未指定のMesh/SpriteRenderer）はアクティブ状態を毎フレーム再確認しつつ
    // ワールド行列を更新する。並び順自体はRebuildCachedEntriesで確定済みのため、ハッシュマップ検索・
    // 文字列コピー・再ソートを毎フレーム行う必要がない
    std::vector<SortableEntry> cachedLive;
    if (editorTarget_ && editorTarget_->IsRenderTargetAvailable()) {
        cachedLive.reserve(cachedEntries_.size());
        for (auto &cached : cachedEntries_) {
            const bool active = std::visit([](auto *r) { return r && r->IsActive(); }, cached.source);
            if (!active) continue;
            SortableEntry ranked = cached.ranked;
            ranked.entry.worldMatrix = std::visit([](auto *r) {
                return Shake::ApplyRenderOnlyOffsets(r->GetOwnerObject(), r->GetWorldMatrix());
            }, cached.source);
            // Instance Colorは実行中にスクリプト等から変更され得るため、ワールド行列と同様に毎フレーム反映する
            ranked.entry.instanceColor = std::visit([](auto *r) { return GetInstanceColorFor(r); }, cached.source);
            ranked.entry.instanceColorBlendMode = std::visit([](auto *r) { return GetInstanceColorBlendModeFor(r); }, cached.source);
            cachedLive.push_back(std::move(ranked));
        }
    }

    // キャッシュ済み（ソート済み）分と毎フレーム収集した分をマージする（両方ソート済みなのでO(n)）
    sortedDrawList_.reserve(cachedLive.size() + freshEntries.size());
    size_t cachedIndex = 0, freshIndex = 0;
    while (cachedIndex < cachedLive.size() && freshIndex < freshEntries.size()) {
        if (CompareSortableEntry(freshEntries[freshIndex], cachedLive[cachedIndex])) {
            sortedDrawList_.push_back(std::move(freshEntries[freshIndex].entry));
            ++freshIndex;
        } else {
            sortedDrawList_.push_back(std::move(cachedLive[cachedIndex].entry));
            ++cachedIndex;
        }
    }
    while (cachedIndex < cachedLive.size()) {
        sortedDrawList_.push_back(std::move(cachedLive[cachedIndex].entry));
        ++cachedIndex;
    }
    while (freshIndex < freshEntries.size()) {
        sortedDrawList_.push_back(std::move(freshEntries[freshIndex].entry));
        ++freshIndex;
    }

    return sortedDrawList_;
}

#if defined(USE_IMGUI)
void SceneRenderer::ShowImGui() {
    ImGui::Text("MeshRenderers: %d", static_cast<int>(meshRenderers_.size()));
    ImGui::Text("SpriteRenderers: %d", static_cast<int>(spriteRenderers_.size()));
    ImGui::Text("TextRenderers: %d", static_cast<int>(textRenderers_.size()));
    ImGui::Text("SkinnedMeshRenderers: %d", static_cast<int>(skinnedMeshRenderers_.size()));
    ImGui::Text("CameraRenderers: %d", static_cast<int>(cameraRenderers_.size()));
    ImGui::Text("LightRenderers: %d", static_cast<int>(lightRenderers_.size()));
    ImGui::Text("%s%d (%s%s)", TranslationC("editor.scenerenderer.cachedentries"),
        static_cast<int>(cachedEntries_.size()), TranslationC("editor.scenerenderer.dirty"),
        drawListDirty_ ? TranslationC("yes") : TranslationC("no"));
}
#endif

} // namespace KashipanEngine
