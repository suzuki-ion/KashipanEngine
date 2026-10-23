#include "RendererInternal.h"

namespace KashipanEngine {

using namespace RendererInternal;

FrameLights &Renderer::PrepareFrameLights(SceneRenderer *sceneRenderer, IRenderTarget *target, const std::string &pipelineName) {
    auto &slot = frameLights_[target][pipelineName];
    if (slot) return *slot;
    slot = std::make_shared<FrameLights>();
    auto &lights = *slot;
    const auto noShadow = [](const LightRenderer *) -> std::int32_t { return -1; };
    CollectLightsForTarget(sceneRenderer, target, pipelineName, noShadow,
        lights.pointLights, lights.spotLights, lights.directionalLights, lights.sphereLights,
        lights.discLights, lights.rectLights, lights.tubeLights, lights.boxLights, &lights.sources);
    auto upload = [&]<typename T>(const std::vector<T> &values, size_t index, const char *suffix) {
        auto key = MakeBatchKey(target, pipelineName, 0, 0, suffix);
        auto *buffer = resourceContainer_->GetOrCreateStructuredBuffer(key, sizeof(T), std::max<size_t>(1, values.size()));
        lights.buffers[index] = buffer;
        if (buffer) if (auto *mapped = static_cast<T *>(buffer->Map())) {
            if (values.empty()) mapped[0] = T{};
            else std::memcpy(mapped, values.data(), sizeof(T) * values.size());
        }
    };
    upload(lights.pointLights, 0, "pointLights");
    upload(lights.spotLights, 1, "spotLights");
    upload(lights.directionalLights, 2, "directionalLights");
    upload(lights.sphereLights, 3, "sphereLights");
    upload(lights.discLights, 4, "discLights");
    upload(lights.rectLights, 5, "rectLights");
    upload(lights.tubeLights, 6, "tubeLights");
    upload(lights.boxLights, 7, "boxLights");
    LightCountsData counts;
    counts.pointLightCount = static_cast<std::uint32_t>(lights.pointLights.size());
    counts.spotLightCount = static_cast<std::uint32_t>(lights.spotLights.size());
    counts.directionalLightCount = static_cast<std::uint32_t>(lights.directionalLights.size());
    counts.sphereLightCount = static_cast<std::uint32_t>(lights.sphereLights.size());
    counts.discLightCount = static_cast<std::uint32_t>(lights.discLights.size());
    counts.rectLightCount = static_cast<std::uint32_t>(lights.rectLights.size());
    counts.tubeLightCount = static_cast<std::uint32_t>(lights.tubeLights.size());
    counts.boxLightCount = static_cast<std::uint32_t>(lights.boxLights.size());
    lights.countsBuffer = resourceContainer_->GetOrCreateConstantBuffer(MakeBatchKey(target, pipelineName, 0, 0, "lightCounts"), sizeof(LightCountsData));
    if (lights.countsBuffer) if (auto *mapped = lights.countsBuffer->Map()) std::memcpy(mapped, &counts, sizeof(counts));
    return lights;
}

void Renderer::ProcessLightCulling(SceneContext *sceneContext, SceneRenderer *sceneRenderer,
    std::span<const SceneRenderer::DrawEntry> drawList) {
    if (!sceneContext || !sceneRenderer) return;
    if (!pipelineManager_->HasPipeline("LightCulling") || pipelineManager_->GetPipeline("LightCulling").Type() != PipelineType::Compute) return;

    // 3D描画（Object3D.*）に使われる (描画先, パイプライン名) の組を重複無く収集する
    // （2D/Skybox/デバッグ/ポストプロセスのパイプラインはライティングを行わないため対象外）
    std::vector<std::pair<IRenderTarget *, std::string>> targetPipelinePairs;
    for (const auto &entry : drawList) {
        if (!entry.target || entry.pipelineName.rfind("Object3D.", 0) != 0) continue;
        const bool alreadyAdded = std::any_of(targetPipelinePairs.begin(), targetPipelinePairs.end(),
            [&](const auto &pair) { return pair.first == entry.target && pair.second == entry.pipelineName; });
        if (!alreadyAdded) targetPipelinePairs.emplace_back(entry.target, entry.pipelineName);
    }
    if (targetPipelinePairs.empty()) return;

    auto *commandList = ComputeCommandProcessor::GetCommandList(Passkey<Renderer>{});
    if (!commandList) return;

    // 共有コマンドリスト上でもフェーズ開始時のパイプライン状態は明示的に作り直す
    PipelineBinder pipelineBinder(commandList, pipelineManager_);
    pipelineBinder.Invalidate();

    for (const auto &[target, pipelineName] : targetPipelinePairs) {
        const std::uint32_t width = target->GetRenderTargetWidth();
        const std::uint32_t height = target->GetRenderTargetHeight();
        if (width == 0 || height == 0) continue;
        const std::uint32_t tileCountX = (width + kTileSize - 1) / kTileSize;
        const std::uint32_t tileCountY = (height + kTileSize - 1) / kTileSize;

        auto *cameraConstantBuffer = ResolveCameraConstantBuffer(sceneRenderer, target, pipelineName);
        if (!cameraConstantBuffer) continue;

        auto &shared = PrepareFrameLights(sceneRenderer, target, pipelineName);
        const auto &pointLights = shared.pointLights;
        const auto &spotLights = shared.spotLights;
        const auto &sphereLights = shared.sphereLights;
        const auto &discLights = shared.discLights;
        const auto &rectLights = shared.rectLights;
        const auto &tubeLights = shared.tubeLights;
        const auto &boxLights = shared.boxLights;
        auto *pointBuffer = shared.buffers[0];
        auto *spotBuffer = shared.buffers[1];
        auto *sphereBuffer = shared.buffers[3];
        auto *discBuffer = shared.buffers[4];
        auto *rectBuffer = shared.buffers[5];
        auto *tubeBuffer = shared.buffers[6];
        auto *boxBuffer = shared.buffers[7];
        if (!pointBuffer || !spotBuffer || !sphereBuffer || !discBuffer || !rectBuffer || !tubeBuffer || !boxBuffer) continue;

        TileCullingConstants constants;
        constants.screenSize = Vector2(static_cast<float>(width), static_cast<float>(height));
        constants.tileCountX = tileCountX;
        constants.tileCountY = tileCountY;
        constants.pointLightCount = static_cast<std::uint32_t>(pointLights.size());
        constants.spotLightCount = static_cast<std::uint32_t>(spotLights.size());
        constants.sphereLightCount = static_cast<std::uint32_t>(sphereLights.size());
        constants.discLightCount = static_cast<std::uint32_t>(discLights.size());
        constants.rectLightCount = static_cast<std::uint32_t>(rectLights.size());
        constants.tubeLightCount = static_cast<std::uint32_t>(tubeLights.size());
        constants.boxLightCount = static_cast<std::uint32_t>(boxLights.size());
        constants.maxLightsPerTile = kMaxLightsPerTile;
        constants.tileSize = kTileSize;
        auto constantsKey = MakeBatchKey(target, pipelineName, 0, 0, "tileCullingConstants");
        auto *constantsBuffer = resourceContainer_->GetOrCreateConstantBuffer(constantsKey, sizeof(TileCullingConstants));
        if (!constantsBuffer) continue;
        if (auto *mapped = constantsBuffer->Map()) {
            std::memcpy(mapped, &constants, sizeof(constants));
        }

        const size_t tileElementCount = static_cast<size_t>(tileCountX) * tileCountY * (1 + kMaxLightsPerTile);
        auto tileKey = MakeBatchKey(target, pipelineName, 0, 0, "tileLightIndices");
        auto *tileBuffer = resourceContainer_->GetOrCreateRWStructuredBufferSrv(tileKey, sizeof(std::uint32_t), tileElementCount);
        if (!tileBuffer) continue;

        pipelineBinder.UsePipeline("LightCulling");
        auto &shaderBinder = pipelineManager_->GetShaderVariableBinder(Passkey<Renderer>{}, "LightCulling");
        shaderBinder.SetCommandList(commandList);

        tileBuffer->SetCommandList(commandList);
        tileBuffer->TransitionTo(D3D12_RESOURCE_STATE_UNORDERED_ACCESS);

        shaderBinder.Bind("Compute:TileCullingConstants", constantsBuffer);
        shaderBinder.Bind("Compute:gCamera3D", cameraConstantBuffer);
        shaderBinder.Bind("Compute:gPointLights", pointBuffer);
        shaderBinder.Bind("Compute:gSpotLights", spotBuffer);
        shaderBinder.Bind("Compute:gSphereLights", sphereBuffer);
        shaderBinder.Bind("Compute:gDiscLights", discBuffer);
        shaderBinder.Bind("Compute:gRectLights", rectBuffer);
        shaderBinder.Bind("Compute:gTubeLights", tubeBuffer);
        shaderBinder.Bind("Compute:gBoxLights", boxBuffer);
        shaderBinder.Bind("Compute:gTileLightIndices", tileBuffer);

        const std::uint32_t groupX = (tileCountX + 7) / 8;
        const std::uint32_t groupY = (tileCountY + 7) / 8;
        commandList->Dispatch(std::max(1u, groupX), std::max(1u, groupY), 1);

        // 後続の描画パスでピクセルシェーダーからStructuredBufferとして読めるように状態遷移しておく
        tileBuffer->TransitionTo(D3D12_RESOURCE_STATE_NON_PIXEL_SHADER_RESOURCE | D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE);
    }

}


void Renderer::BindCameraAndLights(ID3D12GraphicsCommandList *commandList,
    IRenderTarget *target,
    const std::string &pipelineName,
    SceneRenderer *sceneRenderer,
    PipelineBinder &pipelineBinder,
    CameraLightsBindCache &lightsCache) {
    // 直前のバインドと同じパイプラインのままで、かつその間にパイプラインの実切り替え
    // （ルートシグネチャの変更）が一度も起きていなければ、ルート引数（カメラ・ライト）は
    // まだ有効なはずなので再バインドをスキップする（同一(target, pipeline)のバッチが
    // 連続する間、毎バッチ発生していたライト一覧再構築・GPUバッファ再アップロードを防ぐ）
    if (lightsCache.valid && lightsCache.pipelineName == pipelineName && lightsCache.generation == pipelineBinder.Generation()) {
        return;
    }
    lightsCache.pipelineName = pipelineName;
    lightsCache.generation = pipelineBinder.Generation();
    lightsCache.valid = true;

    auto &shaderBinder = pipelineManager_->GetShaderVariableBinder(Passkey<Renderer>{}, pipelineName);
    shaderBinder.SetCommandList(commandList);

    // エディター用描画先の場合はエディターカメラ（フリーカム）をgCamera3Dへ優先バインドする。
    // ただしこれは3Dカメラの代替に過ぎないため、以前はここでelseに入り下のループを
    // 丸ごとスキップしていた。すると同じ描画先に描くObject2D/Text2D等（gCamera2Dを使う）は
    // シーン側のCameraRendererを一切バインドされず、ルート引数未初期化のままDrawされ、
    // GPUベース検証で"Uninitialized root argument accessed"となりクラッシュしていた。
    // gCamera3D分だけ上書きし、それ以外（gCamera2D等）は通常通りシーンのCameraRendererから
    // バインドする
    auto *editorCameraBuffer = sceneRenderer->GetEditorCameraBuffer(target);
    if (editorCameraBuffer) {
        shaderBinder.Bind("Vertex:gCamera3D", editorCameraBuffer);
        shaderBinder.Bind("Pixel:gCamera3D", editorCameraBuffer);
    }
    // シーンビューの表示モードが「2D」の場合のみ、エディター専用のパン・ズームカメラをgCamera2Dへ
    // 優先バインドする（GetEditorCamera2DBufferがモード判定込みで管理。SceneRenderer::EditorDisplayMode
    // 参照）。これによりSpriteRendererは元のパイプライン・投影のまま、シーン内の実際のCamera2D
    // コンポーネントの有無に関係なく、エディター側で自由にパン・ズームして編集できる
    auto *editorCamera2DBuffer = sceneRenderer->GetEditorCamera2DBuffer(target);
    if (editorCamera2DBuffer) {
        shaderBinder.Bind("Vertex:gCamera2D", editorCamera2DBuffer);
        shaderBinder.Bind("Pixel:gCamera2D", editorCamera2DBuffer);
    }
    for (auto *cameraRenderer : sceneRenderer->GetCameraRenderers()) {
        if (!cameraRenderer || !cameraRenderer->IsActive()) continue;
        // EditorOnlyオブジェクトのカメラはエディター用以外の描画先にはバインドしない
        if (IsExcludedAsEditorOnly(cameraRenderer, target, sceneRenderer)) continue;
        // パイプライン指定がある場合は一致するパイプラインのみバインド
        if (!cameraRenderer->GetPipelineName().empty() && cameraRenderer->GetPipelineName() != pipelineName) continue;
        // 描画先指定がある場合は一致する描画先のみバインド
        if (!IsTargetMatch(cameraRenderer->GetTargetObject(), cameraRenderer->GetTargetObjectID().IsValid(), target, sceneRenderer)) continue;
        // 除外設定されている描画先にはバインドしない
        if (!cameraRenderer->IsRenderTargetIncluded(target)) continue;
        auto *constantBuffer = cameraRenderer->GetConstantBuffer();
        if (!constantBuffer) continue;
        for (const auto &variableName : cameraRenderer->GetBindVariableNames()) {
            // gCamera3D/gCamera2Dはエディターカメラで上書き済みなので二重バインドしない
            if (editorCameraBuffer && (variableName == "Vertex:gCamera3D" || variableName == "Pixel:gCamera3D")) continue;
            if (editorCamera2DBuffer && (variableName == "Vertex:gCamera2D" || variableName == "Pixel:gCamera2D")) continue;
            shaderBinder.Bind(variableName, constantBuffer);
        }
    }

    // グローバル時刻定数（Object2D等の時間ベース演出用）。全描画先・全パイプラインで共通の値のため、
    // targetやpipelineNameに依存しない固定キーでキャッシュする
    {
        TimeConstantsData timeData;
        timeData.time = static_cast<float>(GetGameRuntimeMillisecond()) / 1000.0f;
        timeData.deltaTime = GetDeltaTime();
        auto *timeBuffer = resourceContainer_->GetOrCreateConstantBuffer("GlobalTimeConstants", sizeof(TimeConstantsData));
        if (timeBuffer) {
            if (auto *mapped = timeBuffer->Map()) {
                std::memcpy(mapped, &timeData, sizeof(timeData));
                shaderBinder.Bind("Vertex:TimeConstants", timeBuffer);
                shaderBinder.Bind("Pixel:TimeConstants", timeBuffer);
            }
        }
    }

    // ブルーノイズによるディザ閾値テーブル（起動時に1回だけ生成済み。全描画先・全パイプラインで
    // 共通のため、こちらもtargetやpipelineNameに依存せず無条件でバインドする）
    if (blueNoiseGenerator_.IsReady()) {
        shaderBinder.Bind("Pixel:gBlueNoiseDither", blueNoiseGenerator_.GetResultBuffer());
    }

    // ライトは種類ごとに構造化バッファへまとめてバインドする
    BindLightBuffersAndShadowMap(target, pipelineName, sceneRenderer, shaderBinder);
}

void Renderer::BindLightBuffersAndShadowMap(IRenderTarget *target,
    const std::string &pipelineName,
    SceneRenderer *sceneRenderer,
    ShaderVariableBinder &shaderBinder) {
    //--------- この描画先の「影を生成するライト」割り当て（RenderShadowMapsで構築済み） ---------//
    const std::vector<TargetShadowEntry> *shadowEntries = nullptr;
    if (auto it = targetShadowEntries_.find(target); it != targetShadowEntries_.end()) {
        shadowEntries = &it->second;
    }
    const auto findShadowIndex = [this, shadowEntries](const LightRenderer *lightRenderer) -> std::int32_t {
        if (!shadowEntries || !shadowArrayReady_) return -1;
        for (size_t i = 0; i < shadowEntries->size(); ++i) {
            if ((*shadowEntries)[i].lightRenderer == lightRenderer) return static_cast<std::int32_t>(i);
        }
        return -1;
    };

    auto &shared = PrepareFrameLights(sceneRenderer, target, pipelineName);
    if (!shared.shadowIndicesPatched) {
        // Culling never reads shadow indices. Once shadow jobs are prepared, patch
        // only those fields in the same upload buffers; do not recollect/reupload lights.
        auto patch = [&]<typename T>(std::vector<T> &values, size_t index) {
            T *mapped = nullptr;
            for (size_t i = 0; i < values.size(); ++i) {
                const auto slot = findShadowIndex(shared.sources[index][i]);
                if (values[i].shadowMapIndex == slot) continue;
                values[i].shadowMapIndex = slot;
                if (!mapped && shared.buffers[index]) mapped = static_cast<T *>(shared.buffers[index]->Map());
                if (mapped) mapped[i].shadowMapIndex = slot;
            }
        };
        patch(shared.pointLights, 0);
        patch(shared.spotLights, 1);
        patch(shared.directionalLights, 2);
        patch(shared.sphereLights, 3);
        patch(shared.discLights, 4);
        patch(shared.rectLights, 5);
        patch(shared.tubeLights, 6);
        patch(shared.boxLights, 7);
        shared.shadowIndicesPatched = true;
    }
    if (shared.countsBuffer) shaderBinder.Bind("Pixel:LightCounts", shared.countsBuffer);
    if (shared.buffers[0]) shaderBinder.Bind("Pixel:gPointLights", shared.buffers[0]);
    if (shared.buffers[1]) shaderBinder.Bind("Pixel:gSpotLights", shared.buffers[1]);
    if (shared.buffers[2]) shaderBinder.Bind("Pixel:gDirectionalLights", shared.buffers[2]);
    if (shared.buffers[3]) shaderBinder.Bind("Pixel:gSphereLights", shared.buffers[3]);
    if (shared.buffers[4]) shaderBinder.Bind("Pixel:gDiscLights", shared.buffers[4]);
    if (shared.buffers[5]) shaderBinder.Bind("Pixel:gRectLights", shared.buffers[5]);
    if (shared.buffers[6]) shaderBinder.Bind("Pixel:gTubeLights", shared.buffers[6]);
    if (shared.buffers[7]) shaderBinder.Bind("Pixel:gBoxLights", shared.buffers[7]);

    //--------- シャドウマップ ---------//
    // RenderShadowMaps() で構築した「この描画先で影を生成するライト」の行列・スライス情報と
    // 全シャドウマップをまとめたTexture2DArrayをバインドする
    {
        ShadowMapConstantsData shadowConstants{};
        const std::uint32_t shadowLightCount = (shadowEntries && shadowArrayReady_)
            ? static_cast<std::uint32_t>(shadowEntries->size())
            : 0u;
        shadowConstants.shadowLightCount = shadowLightCount;
        const float texel = 1.0f / static_cast<float>(std::max(1u, shadowArrayResolution_));
        for (std::uint32_t slot = 0; slot < shadowLightCount; ++slot) {
            const int jobIndex = (*shadowEntries)[slot].jobIndex;
            if (jobIndex < 0 || jobIndex >= static_cast<int>(shadowJobs_.size())) continue;
            const auto &job = shadowJobs_[jobIndex];
            auto &destination = shadowConstants.lights[slot];
            for (std::uint32_t v = 0; v < kMaxShadowViewProjections; ++v) {
                destination.viewProjections[v] = job.viewProjections[v];
            }
            for (std::uint32_t c = 0; c < kShadowCascadeCount; ++c) {
                destination.cascadeSplits[c] = job.cascadeSplits[c];
                destination.cascadeBiasScales[c] = job.cascadeBiasScales[c];
            }
            destination.params[0] = texel;
            destination.params[1] = static_cast<float>(job.baseSlice);
            destination.params[2] = static_cast<float>(job.lightType);
            destination.params[3] = job.perspectiveBiasScale;

            // 光源サイズに応じた半影のソフト化（PCSS）に使うワールド単位の光源サイズ
            const auto *lightRenderer = (*shadowEntries)[slot].lightRenderer;
            const auto *light = lightRenderer ? lightRenderer->GetLight() : nullptr;
            destination.pcssParams[0] = light ? light->GetEffectiveShadowSoftness() : 0.0f;
        }

        auto key = MakeBatchKey(target, pipelineName, 0, 0, "shadowMapConstants");
        auto *constantsBuffer = resourceContainer_->GetOrCreateConstantBuffer(key, sizeof(ShadowMapConstantsData));
        if (constantsBuffer) {
            if (auto *mapped = constantsBuffer->Map()) {
                std::memcpy(mapped, &shadowConstants, sizeof(shadowConstants));
                shaderBinder.Bind("Pixel:ShadowMapConstants", constantsBuffer);
            }
        }

        // シャドウマップ配列（影ジョブが無いフレームでもダミー配列がSRV状態で維持されている）
        if (shadowMapArray_ && shadowArrayReady_) {
            shaderBinder.Bind("Pixel:gShadowMaps", shadowMapArray_->GetSrvGPUHandle());
        }
    }
    SamplerManager::BindSampler(&shaderBinder, "Pixel:gShadowSamplerCmp", GetShadowSamplerCmpHandle());
    // PCSSのブロッカーサーチ（比較無しで生の深度値を読む）用の点サンプラー
    SamplerManager::BindSampler(&shaderBinder, "Pixel:gShadowSamplerPoint", DefaultSampler::PointClamp);

    //--------- Forward+ タイルライトカリング結果（ProcessLightCullingが計算済みのものを読むだけ） ---------//
    if (pipelineName.rfind("Object3D.", 0) == 0) {
        const std::uint32_t width = target ? target->GetRenderTargetWidth() : 0;
        const std::uint32_t height = target ? target->GetRenderTargetHeight() : 0;
        const std::uint32_t tileCountX = (width + kTileSize - 1) / kTileSize;
        const std::uint32_t tileCountY = (height + kTileSize - 1) / kTileSize;
        if (tileCountX > 0 && tileCountY > 0) {
            const size_t tileElementCount = static_cast<size_t>(tileCountX) * tileCountY * (1 + kMaxLightsPerTile);
            auto tileKey = MakeBatchKey(target, pipelineName, 0, 0, "tileLightIndices");
            auto *tileBuffer = resourceContainer_->GetOrCreateRWStructuredBufferSrv(tileKey, sizeof(std::uint32_t), tileElementCount);
            if (tileBuffer) {
                shaderBinder.Bind("Pixel:gTileLightIndices", tileBuffer);
            }
            auto constantsKey = MakeBatchKey(target, pipelineName, 0, 0, "tileCullingConstants");
            auto *tileConstantsBuffer = resourceContainer_->GetOrCreateConstantBuffer(constantsKey, sizeof(TileCullingConstants));
            if (tileConstantsBuffer) {
                shaderBinder.Bind("Pixel:TileCullingConstants", tileConstantsBuffer);
            }
        }
    }
}


} // namespace KashipanEngine
