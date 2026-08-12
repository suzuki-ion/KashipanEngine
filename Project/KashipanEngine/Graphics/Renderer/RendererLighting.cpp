#include "RendererInternal.h"

namespace KashipanEngine {

using namespace RendererInternal;

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

        // この描画先・パイプラインで影を生成するライトの割り当て（RenderShadowMapsは後で実行されるため、
        // シャドウスロット自体はここでは未確定。ライトカリングは影の有無に関係なく行えるため -1 固定でよい）
        const auto noShadow = [](const LightRenderer *) -> std::int32_t { return -1; };

        std::vector<PointLightElement> pointLights;
        std::vector<SpotLightElement> spotLights;
        std::vector<DirectionalLightElement> directionalLights;
        std::vector<SphereLightElement> sphereLights;
        std::vector<DiscLightElement> discLights;
        std::vector<RectLightElement> rectLights;
        std::vector<TubeLightElement> tubeLights;
        std::vector<BoxLightElement> boxLights;
        CollectLightsForTarget(sceneRenderer, target, pipelineName, noShadow, pointLights, spotLights, directionalLights,
            sphereLights, discLights, rectLights, tubeLights, boxLights);
        // ライトが0個でも必ずディスパッチする（gTileLightIndicesはDEFAULTヒープ上のUAVでCPUから初期化できないため、
        // ここで確実に書き込んでおかないとBindLightBuffersAndShadowMap側で未初期化のバッファを読むことになる）

        auto pointKey = MakeBatchKey(target, pipelineName, 0, 0, "pointLights");
        auto *pointBuffer = resourceContainer_->GetOrCreateStructuredBuffer(
            pointKey, sizeof(PointLightElement), std::max<size_t>(1, pointLights.size()));
        auto spotKey = MakeBatchKey(target, pipelineName, 0, 0, "spotLights");
        auto *spotBuffer = resourceContainer_->GetOrCreateStructuredBuffer(
            spotKey, sizeof(SpotLightElement), std::max<size_t>(1, spotLights.size()));
        auto sphereKey = MakeBatchKey(target, pipelineName, 0, 0, "sphereLights");
        auto *sphereBuffer = resourceContainer_->GetOrCreateStructuredBuffer(
            sphereKey, sizeof(SphereLightElement), std::max<size_t>(1, sphereLights.size()));
        auto discKey = MakeBatchKey(target, pipelineName, 0, 0, "discLights");
        auto *discBuffer = resourceContainer_->GetOrCreateStructuredBuffer(
            discKey, sizeof(DiscLightElement), std::max<size_t>(1, discLights.size()));
        auto rectKey = MakeBatchKey(target, pipelineName, 0, 0, "rectLights");
        auto *rectBuffer = resourceContainer_->GetOrCreateStructuredBuffer(
            rectKey, sizeof(RectLightElement), std::max<size_t>(1, rectLights.size()));
        auto tubeKey = MakeBatchKey(target, pipelineName, 0, 0, "tubeLights");
        auto *tubeBuffer = resourceContainer_->GetOrCreateStructuredBuffer(
            tubeKey, sizeof(TubeLightElement), std::max<size_t>(1, tubeLights.size()));
        auto boxKey = MakeBatchKey(target, pipelineName, 0, 0, "boxLights");
        auto *boxBuffer = resourceContainer_->GetOrCreateStructuredBuffer(
            boxKey, sizeof(BoxLightElement), std::max<size_t>(1, boxLights.size()));
        if (!pointBuffer || !spotBuffer || !sphereBuffer || !discBuffer || !rectBuffer || !tubeBuffer || !boxBuffer) continue;
        if (auto *mapped = static_cast<PointLightElement *>(pointBuffer->Map())) {
            if (pointLights.empty()) mapped[0] = PointLightElement{};
            else std::memcpy(mapped, pointLights.data(), sizeof(PointLightElement) * pointLights.size());
        }
        if (auto *mapped = static_cast<SpotLightElement *>(spotBuffer->Map())) {
            if (spotLights.empty()) mapped[0] = SpotLightElement{};
            else std::memcpy(mapped, spotLights.data(), sizeof(SpotLightElement) * spotLights.size());
        }
        if (auto *mapped = static_cast<SphereLightElement *>(sphereBuffer->Map())) {
            if (sphereLights.empty()) mapped[0] = SphereLightElement{};
            else std::memcpy(mapped, sphereLights.data(), sizeof(SphereLightElement) * sphereLights.size());
        }
        if (auto *mapped = static_cast<DiscLightElement *>(discBuffer->Map())) {
            if (discLights.empty()) mapped[0] = DiscLightElement{};
            else std::memcpy(mapped, discLights.data(), sizeof(DiscLightElement) * discLights.size());
        }
        if (auto *mapped = static_cast<RectLightElement *>(rectBuffer->Map())) {
            if (rectLights.empty()) mapped[0] = RectLightElement{};
            else std::memcpy(mapped, rectLights.data(), sizeof(RectLightElement) * rectLights.size());
        }
        if (auto *mapped = static_cast<TubeLightElement *>(tubeBuffer->Map())) {
            if (tubeLights.empty()) mapped[0] = TubeLightElement{};
            else std::memcpy(mapped, tubeLights.data(), sizeof(TubeLightElement) * tubeLights.size());
        }
        if (auto *mapped = static_cast<BoxLightElement *>(boxBuffer->Map())) {
            if (boxLights.empty()) mapped[0] = BoxLightElement{};
            else std::memcpy(mapped, boxLights.data(), sizeof(BoxLightElement) * boxLights.size());
        }

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
            // gCamera3Dはエディターカメラで上書き済みなので二重バインドしない
            if (editorCameraBuffer && (variableName == "Vertex:gCamera3D" || variableName == "Pixel:gCamera3D")) continue;
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

    //--------- ライトの収集（種類ごとに構造化バッファへまとめる） ---------//
    std::vector<PointLightElement> pointLights;
    std::vector<SpotLightElement> spotLights;
    std::vector<DirectionalLightElement> directionalLights;
    std::vector<SphereLightElement> sphereLights;
    std::vector<DiscLightElement> discLights;
    std::vector<RectLightElement> rectLights;
    std::vector<TubeLightElement> tubeLights;
    std::vector<BoxLightElement> boxLights;
    CollectLightsForTarget(sceneRenderer, target, pipelineName, findShadowIndex, pointLights, spotLights, directionalLights,
        sphereLights, discLights, rectLights, tubeLights, boxLights);

    //--------- ライト個数の定数バッファ ---------//
    {
        LightCountsData counts;
        counts.pointLightCount = static_cast<std::uint32_t>(pointLights.size());
        counts.spotLightCount = static_cast<std::uint32_t>(spotLights.size());
        counts.directionalLightCount = static_cast<std::uint32_t>(directionalLights.size());
        counts.sphereLightCount = static_cast<std::uint32_t>(sphereLights.size());
        counts.discLightCount = static_cast<std::uint32_t>(discLights.size());
        counts.rectLightCount = static_cast<std::uint32_t>(rectLights.size());
        counts.tubeLightCount = static_cast<std::uint32_t>(tubeLights.size());
        counts.boxLightCount = static_cast<std::uint32_t>(boxLights.size());
        auto key = MakeBatchKey(target, pipelineName, 0, 0, "lightCounts");
        auto *countsBuffer = resourceContainer_->GetOrCreateConstantBuffer(key, sizeof(LightCountsData));
        if (countsBuffer) {
            if (auto *mapped = countsBuffer->Map()) {
                std::memcpy(mapped, &counts, sizeof(counts));
                shaderBinder.Bind("Pixel:LightCounts", countsBuffer);
            }
        }
    }

    //--------- ライトの構造化バッファ（0個でもダミー1要素をバインドする） ---------//
    {
        auto key = MakeBatchKey(target, pipelineName, 0, 0, "pointLights");
        auto *buffer = resourceContainer_->GetOrCreateStructuredBuffer(
            key, sizeof(PointLightElement), std::max<size_t>(1, pointLights.size()));
        if (buffer) {
            if (auto *mapped = static_cast<PointLightElement *>(buffer->Map())) {
                if (pointLights.empty()) {
                    mapped[0] = PointLightElement{};
                } else {
                    std::memcpy(mapped, pointLights.data(), sizeof(PointLightElement) * pointLights.size());
                }
                shaderBinder.Bind("Pixel:gPointLights", buffer);
            }
        }
    }
    {
        auto key = MakeBatchKey(target, pipelineName, 0, 0, "spotLights");
        auto *buffer = resourceContainer_->GetOrCreateStructuredBuffer(
            key, sizeof(SpotLightElement), std::max<size_t>(1, spotLights.size()));
        if (buffer) {
            if (auto *mapped = static_cast<SpotLightElement *>(buffer->Map())) {
                if (spotLights.empty()) {
                    mapped[0] = SpotLightElement{};
                } else {
                    std::memcpy(mapped, spotLights.data(), sizeof(SpotLightElement) * spotLights.size());
                }
                shaderBinder.Bind("Pixel:gSpotLights", buffer);
            }
        }
    }
    {
        auto key = MakeBatchKey(target, pipelineName, 0, 0, "directionalLights");
        auto *buffer = resourceContainer_->GetOrCreateStructuredBuffer(
            key, sizeof(DirectionalLightElement), std::max<size_t>(1, directionalLights.size()));
        if (buffer) {
            if (auto *mapped = static_cast<DirectionalLightElement *>(buffer->Map())) {
                if (directionalLights.empty()) {
                    mapped[0] = DirectionalLightElement{};
                } else {
                    std::memcpy(mapped, directionalLights.data(), sizeof(DirectionalLightElement) * directionalLights.size());
                }
                shaderBinder.Bind("Pixel:gDirectionalLights", buffer);
            }
        }
    }
    {
        auto key = MakeBatchKey(target, pipelineName, 0, 0, "sphereLights");
        auto *buffer = resourceContainer_->GetOrCreateStructuredBuffer(
            key, sizeof(SphereLightElement), std::max<size_t>(1, sphereLights.size()));
        if (buffer) {
            if (auto *mapped = static_cast<SphereLightElement *>(buffer->Map())) {
                if (sphereLights.empty()) mapped[0] = SphereLightElement{};
                else std::memcpy(mapped, sphereLights.data(), sizeof(SphereLightElement) * sphereLights.size());
                shaderBinder.Bind("Pixel:gSphereLights", buffer);
            }
        }
    }
    {
        auto key = MakeBatchKey(target, pipelineName, 0, 0, "discLights");
        auto *buffer = resourceContainer_->GetOrCreateStructuredBuffer(
            key, sizeof(DiscLightElement), std::max<size_t>(1, discLights.size()));
        if (buffer) {
            if (auto *mapped = static_cast<DiscLightElement *>(buffer->Map())) {
                if (discLights.empty()) mapped[0] = DiscLightElement{};
                else std::memcpy(mapped, discLights.data(), sizeof(DiscLightElement) * discLights.size());
                shaderBinder.Bind("Pixel:gDiscLights", buffer);
            }
        }
    }
    {
        auto key = MakeBatchKey(target, pipelineName, 0, 0, "rectLights");
        auto *buffer = resourceContainer_->GetOrCreateStructuredBuffer(
            key, sizeof(RectLightElement), std::max<size_t>(1, rectLights.size()));
        if (buffer) {
            if (auto *mapped = static_cast<RectLightElement *>(buffer->Map())) {
                if (rectLights.empty()) mapped[0] = RectLightElement{};
                else std::memcpy(mapped, rectLights.data(), sizeof(RectLightElement) * rectLights.size());
                shaderBinder.Bind("Pixel:gRectLights", buffer);
            }
        }
    }
    {
        auto key = MakeBatchKey(target, pipelineName, 0, 0, "tubeLights");
        auto *buffer = resourceContainer_->GetOrCreateStructuredBuffer(
            key, sizeof(TubeLightElement), std::max<size_t>(1, tubeLights.size()));
        if (buffer) {
            if (auto *mapped = static_cast<TubeLightElement *>(buffer->Map())) {
                if (tubeLights.empty()) mapped[0] = TubeLightElement{};
                else std::memcpy(mapped, tubeLights.data(), sizeof(TubeLightElement) * tubeLights.size());
                shaderBinder.Bind("Pixel:gTubeLights", buffer);
            }
        }
    }
    {
        auto key = MakeBatchKey(target, pipelineName, 0, 0, "boxLights");
        auto *buffer = resourceContainer_->GetOrCreateStructuredBuffer(
            key, sizeof(BoxLightElement), std::max<size_t>(1, boxLights.size()));
        if (buffer) {
            if (auto *mapped = static_cast<BoxLightElement *>(buffer->Map())) {
                if (boxLights.empty()) mapped[0] = BoxLightElement{};
                else std::memcpy(mapped, boxLights.data(), sizeof(BoxLightElement) * boxLights.size());
                shaderBinder.Bind("Pixel:gBoxLights", buffer);
            }
        }
    }

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
