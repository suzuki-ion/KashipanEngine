#include "RendererInternal.h"
#include "Debug/Logger.h"
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

    // 破棄済みScreenBuffer向けのRenderMultiPassDitherスクラッチをGCする（描画先のリサイズや
    // シーン切り替えでキーが増え続けるのを防ぐ。詳細はmultiPassDitherScratch_のコメント参照）
    for (auto it = multiPassDitherScratch_.begin(); it != multiPassDitherScratch_.end(); ) {
        if (!ScreenBuffer::IsExist(it->first)) {
            it = multiPassDitherScratch_.erase(it);
        } else {
            ++it;
        }
    }

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

    // デバッグ調査用：ScreenBufferObjectへの描画エントリが継続的にゼロになっていないか追跡する
    TrackZeroDrawEntryTargets(sceneContext, drawList);
}

void Renderer::TrackZeroDrawEntryTargets(SceneContext *sceneContext, std::span<const SceneRenderer::DrawEntry> drawList) {
    if (!sceneContext) return;

    // 描画先ポインタごとの今フレームの描画エントリ数を集計する
    std::unordered_map<const IRenderTarget *, std::uint32_t> countByTarget;
    for (const auto &entry : drawList) {
        ++countByTarget[entry.target];
    }

    // 約0.5秒（60fps基準）連続で描画エントリ0件が続いたら異常とみなす。
    // Play/Stopのたびにフレームレートが一時的に落ちることもあるため、余裕を持たせている
    constexpr std::uint32_t kZeroFrameThreshold = 30;

    for (auto *object : sceneContext->GetSceneObjects()) {
        if (!object || !object->IsActive()) continue;
        for (auto *screenBufferObject : object->GetComponents<ScreenBufferObject>()) {
            if (!screenBufferObject || !screenBufferObject->IsActive()) continue;
            auto *buffer = screenBufferObject->GetScreenBuffer();
            if (!buffer || !ScreenBuffer::IsExist(buffer)) continue;
            if (!buffer->IsRenderTargetAvailable()) continue;

            const std::string &name = screenBufferObject->GetName();
            if (name.empty()) continue;

            const auto it = countByTarget.find(buffer);
            const std::uint32_t count = (it != countByTarget.end()) ? it->second : 0;

            auto &streak = zeroDrawEntryStreak_[name];
            auto &logged = zeroDrawEntryLogged_[name];

            if (count == 0) {
                if (streak < kZeroFrameThreshold) ++streak;
                if (streak >= kZeroFrameThreshold && !logged) {
                    // ScreenBuffer自体は正常（previewReady_も立ちうる）だが、実際の描画エントリが
                    // 一定時間ゼロのまま＝カメラ等の対象解決が外れて何も描かれていない状態。
                    // ポストエフェクトが付いていればRenderPostProcessOnlyTargets経由で
                    // クリア色のまま「準備済み」にされるため、SRV未準備の表示にもならず
                    // ただ透明に見える、という症状の切り分け用ログ
                    Log(Translation("engine.renderer.zerodrawentries.detected") + name, LogSeverity::Warning);
                    logged = true;
                }
            } else {
                if (logged) {
                    Log(Translation("engine.renderer.zerodrawentries.recovered") + name, LogSeverity::Info);
                }
                streak = 0;
                logged = false;
            }
        }
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
