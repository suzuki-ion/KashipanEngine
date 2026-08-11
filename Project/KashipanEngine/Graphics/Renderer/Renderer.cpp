#include "RendererInternal.h"
#include <optional>

namespace KashipanEngine {

using namespace RendererInternal;

Renderer::Renderer(Passkey<GraphicsEngine>, DirectXCommon *directXCommon, PipelineManager *pipelineManager)
    : directXCommon_(directXCommon), pipelineManager_(pipelineManager) {
    resourceContainer_ = std::make_unique<ResourceContainer>();
    // ブルーノイズによるディザ閾値テーブルはエンジン起動時に1回だけ生成すればよい
    // （毎フレーム再生成する必要は無い）ため、ここで同期的に生成しておく
    blueNoiseGenerator_.Generate(directXCommon_, pipelineManager_, Passkey<Renderer>{});
}

Renderer::~Renderer() {
    shadowMapArray_.reset();
    if (directXCommon_ && shadowCommandSlotIndex_ >= 0) {
        directXCommon_->ReleaseCommandObjects(Passkey<Renderer>{}, shadowCommandSlotIndex_);
        shadowCommandSlotIndex_ = -1;
        shadowCommands_ = nullptr;
    }
}

void Renderer::RenderFrame(Passkey<GraphicsEngine>, SceneContext *sceneContext) {
    drawCallCount_ = 0;
    if (!sceneContext || !pipelineManager_) return;

    ComputeCommandProcessor::BeginFrame(Passkey<Renderer>{});

    // Computeシェーダー処理は他の描画パスより先に実行し、結果を後続パスから参照できるようにする
    ProcessComputeShaders(sceneContext);
    // 動画のYUV→RGB変換も同様に描画リスト構築より先に実行する（シーンに依存しない）
    ProcessVideoConversions();
    // GPUスキニングも描画リスト構築より先に実行し、スキニング結果を描画パスから参照できるようにする
    ProcessSkinning(sceneContext);
    // GPUパーティクルも同様に描画リスト構築より先に実行する
    ProcessGpuParticles(sceneContext);

    auto *sceneRenderer = sceneContext->GetComponent<SceneRenderer>();
    if (!sceneRenderer) {
        ComputeCommandProcessor::EndFrame(Passkey<Renderer>{});
        return;
    }

    // カメラの定数バッファは常に最新のTransformを反映する（ゲームループが停止/一時停止中でも
    // 描画自体は継続されるため、Update() 頼みだと停止中はカメラが固まって描画が崩れてしまう）
    for (auto *cameraRenderer : sceneRenderer->GetCameraRenderers()) {
        if (cameraRenderer && cameraRenderer->IsActive()) {
            cameraRenderer->RefreshConstantBuffer();
        }
    }

    const auto &drawList = sceneRenderer->BuildSortedDrawList(Passkey<Renderer>{}, pipelineManager_);

    // Forward+のタイルライトカリング（3D描画で使われる (描画先,パイプライン) の組ごとに実行する）
    ProcessLightCulling(sceneContext, sceneRenderer, drawList);

    // 全Computeフェーズを1本のコマンドリストとして閉じ、後続の描画より先に提出する
    ComputeCommandProcessor::EndFrame(Passkey<Renderer>{});

    // 今フレームで描画される描画先ごとに、その描画先で使うカメラ・ライトからシャドウマップを生成する
    // （他の描画パスより先に実行する）
    std::vector<IRenderTarget *> shadowTargets;
    for (const auto &entry : drawList) {
        if (std::find(shadowTargets.begin(), shadowTargets.end(), entry.target) == shadowTargets.end()) {
            shadowTargets.push_back(entry.target);
        }
    }
    RenderShadowMaps(sceneContext, sceneRenderer, shadowTargets);

    // 画面全体Nパスブレンド（ScreenWideDitherBlendEffect）が有効な描画先を収集する。これらは
    // 全描画先で共有するシャドウマップ配列を自分の位相で直接書き換えながら進むため、他の
    // （画面全体Nパスブレンド対象ではない）描画先を全て処理し終えてから最後にまとめて処理する
    const auto screenWideDitherTargets = CollectScreenWideDitherTargets(sceneRenderer, shadowTargets);
    const auto findScreenWideDitherPassCount = [&screenWideDitherTargets](const IRenderTarget *target) -> std::optional<std::uint32_t> {
        for (const auto &request : screenWideDitherTargets) {
            if (request.screenBuffer == target) return request.passCount;
        }
        return std::nullopt;
    };

    // 描画先ごとの範囲に区切って描画（リストは描画先順でソート済み）。画面全体Nパスブレンド対象は
    // ここでは描画せず、範囲だけ控えておいて最後にまとめて処理する
    std::unordered_set<const IRenderTarget *> renderedTargets;
    std::vector<std::pair<ScreenBuffer *, std::span<const SceneRenderer::DrawEntry>>> deferredScreenWideRanges;
    size_t begin = 0;
    while (begin < drawList.size()) {
        IRenderTarget *target = drawList[begin].target;
        size_t end = begin;
        while (end < drawList.size() && drawList[end].target == target) ++end;

        std::span<const SceneRenderer::DrawEntry> range(drawList.data() + begin, end - begin);
        if (findScreenWideDitherPassCount(target)) {
            deferredScreenWideRanges.emplace_back(static_cast<ScreenBuffer *>(target), range);
        } else {
            RenderToTarget(target, range, sceneRenderer);
        }
        renderedTargets.insert(target);
        begin = end;
    }

    for (const auto &[screenBuffer, range] : deferredScreenWideRanges) {
        const auto passCount = findScreenWideDitherPassCount(screenBuffer);
        RenderScreenWideDitherTarget(screenBuffer, range, sceneRenderer, passCount.value_or(4u));
    }

    // 描画対象オブジェクトが無い ScreenBuffer にもポストエフェクトのみ適用する
    RenderPostProcessOnlyTargets(sceneContext, renderedTargets);

    // シーンに描画対象が一つも無い場合でも、エディター用描画先には背景だけは描画する
    auto *editorTarget = sceneRenderer->GetEditorTarget();
    if (editorTarget && editorTarget->IsRenderTargetAvailable() &&
        editorTarget->GetRenderTargetKind() == RenderTargetKind::ScreenBuffer &&
        renderedTargets.find(editorTarget) == renderedTargets.end()) {
        RenderToTarget(editorTarget, {}, sceneRenderer);
    }
}


void Renderer::ReleaseAllResources(Passkey<GraphicsEngine>) {
    if (resourceContainer_) {
        resourceContainer_->Clear();
    }
    shadowJobs_.clear();
    targetShadowEntries_.clear();
    shadowMapArray_.reset();
    shadowArrayResolution_ = 0;
    shadowArraySliceCount_ = 0;
    shadowArrayReady_ = false;
}


} // namespace KashipanEngine
