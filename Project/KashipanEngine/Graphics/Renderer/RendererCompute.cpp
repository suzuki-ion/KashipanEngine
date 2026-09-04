#include "RendererInternal.h"
#include "Assets/VideoManager.h"
#include "Graphics/VideoTexture.h"
#include "Graphics/Resources/ShaderResourceResource.h"
#include "Graphics/Resources/UnorderedAccessResource.h"
#include "Utilities/Translation.h"

namespace KashipanEngine {

using namespace RendererInternal;

void Renderer::ProcessComputeShaders(SceneContext *sceneContext) {
    LogScope scope;
    if (!sceneContext) return;
    auto *sceneProcessor = sceneContext->GetComponent<SceneComputeProcessor>();
    if (!sceneProcessor) return;
    const auto &components = sceneProcessor->GetComputeShaderProcessings();
    if (components.empty()) return;

    auto *commandList = ComputeCommandProcessor::GetCommandList(Passkey<Renderer>{});
    if (!commandList) return;

    // このコマンドリストはフレームごとにResetされるため、ヒープとパイプラインの状態を毎回設定する
    // （RendererShadow.cppの同パターン参照。SRV/UAVをディスクリプタテーブルで束ねるUAVテクスチャを
    // 使う場合、事前にSetDescriptorHeapsでヒープを束ねておかないと不正なディスクリプタ参照になる）
    if (auto *srvHeap = IGraphicsResource::GetSRVHeap(Passkey<Renderer>{})) {
        if (auto *samplerHeap = IGraphicsResource::GetSamplerHeap(Passkey<Renderer>{})) {
            ID3D12DescriptorHeap *heaps[] = { srvHeap->GetDescriptorHeap(), samplerHeap->GetDescriptorHeap() };
            commandList->SetDescriptorHeaps(_countof(heaps), heaps);
        }
    }

    // 共有コマンドリスト上でもフェーズ開始時のパイプライン状態は明示的に作り直す
    PipelineBinder pipelineBinder(commandList, pipelineManager_);
    pipelineBinder.Invalidate();

    for (auto *component : components) {
        if (!component || !component->IsActive()) continue;
        const std::string &pipelineName = component->GetPipelineName();
        if (pipelineName.empty() || !pipelineManager_->HasPipeline(pipelineName)) continue;
        if (pipelineManager_->GetPipeline(pipelineName).Type() != PipelineType::Compute) continue;

        pipelineBinder.UsePipeline(pipelineName);
        auto &shaderBinder = pipelineManager_->GetShaderVariableBinder(Passkey<Renderer>{}, pipelineName);
        shaderBinder.SetCommandList(commandList);

        // 読み取り専用テクスチャ（読み込み済みテクスチャから）
        for (const auto &req : component->GetTextureBindRequirements()) {
            if (req.variableName.empty() || req.textureAssetPath.empty()) continue;
            const auto handle = TextureManager::GetTextureFromAssetPath(req.textureAssetPath);
            if (handle == TextureManager::kInvalidHandle) continue;
            TextureManager::BindTexture(&shaderBinder, req.variableName, handle);
        }

        // 読み書き可能なUAVテクスチャ（エンジン側で生成・管理。キー継続でフレームをまたいで内容が保持される）
        for (const auto &req : component->GetUAVTextureBindRequirements()) {
            if (req.variableName.empty() || req.width == 0 || req.height == 0) continue;
            const std::string key = MakeBatchKey(component, pipelineName, 0, 0, req.variableName.c_str());
            auto *uavTexture = resourceContainer_->GetOrCreateUAVTexture(key, req.width, req.height, UAVFormatFromKind(req.formatKind));
            if (!uavTexture) continue;
            shaderBinder.Bind(req.variableName, uavTexture);
        }

        // 定数バッファ（float配列をそのまま送る）
        for (const auto &req : component->GetConstantBufferBindRequirements()) {
            if (req.variableName.empty() || req.values.empty()) continue;
            const std::string key = MakeBatchKey(component, pipelineName, 0, 0, req.variableName.c_str());
            const size_t byteSize = req.values.size() * sizeof(float);
            auto *constantBuffer = resourceContainer_->GetOrCreateConstantBuffer(key, byteSize);
            if (!constantBuffer) continue;
            void *mapped = constantBuffer->Map();
            if (!mapped) continue;
            std::memcpy(mapped, req.values.data(), byteSize);
            shaderBinder.Bind(req.variableName, constantBuffer);
        }

        std::uint32_t groupX = 1, groupY = 1, groupZ = 1;
        component->GetGroupCounts(groupX, groupY, groupZ);
        commandList->Dispatch(groupX, groupY, groupZ);
    }

}

void Renderer::ProcessVideoConversions() {
    LogScope scope;
    if (!pipelineManager_) return;
    auto pendingConversions = VideoManager::GetPendingConversions(Passkey<Renderer>{});
    if (pendingConversions.empty()) return;
    if (!pipelineManager_->HasPipeline("VideoYUVToRGB") || pipelineManager_->GetPipeline("VideoYUVToRGB").Type() != PipelineType::Compute) return;

    auto *commandList = ComputeCommandProcessor::GetCommandList(Passkey<Renderer>{});
    if (!commandList) return;

    // このコマンドリストはフレームごとにResetされるため、ヒープの状態を毎回設定する
    // （RendererShadow.cppの同パターン参照）。gTextureY/gTextureUV/gOutputはディスクリプタテーブルで
    // バインドするため、事前にSetDescriptorHeapsでSRVヒープを束ねておかないと不正なディスクリプタ
    // 参照になりドライバがクラッシュする（既存のCompute系パイプラインはすべてルートディスクリプタ
    // 方式で構造化バッファのみを扱っておりディスクリプタヒープを必要としなかったため、
    // このコマンドリストでSetDescriptorHeapsが一度も呼ばれていなかった）
    if (auto *srvHeap = IGraphicsResource::GetSRVHeap(Passkey<Renderer>{})) {
        if (auto *samplerHeap = IGraphicsResource::GetSamplerHeap(Passkey<Renderer>{})) {
            ID3D12DescriptorHeap *heaps[] = { srvHeap->GetDescriptorHeap(), samplerHeap->GetDescriptorHeap() };
            commandList->SetDescriptorHeaps(_countof(heaps), heaps);
        }
    }

    PipelineBinder pipelineBinder(commandList, pipelineManager_);
    pipelineBinder.Invalidate();
    pipelineBinder.UsePipeline("VideoYUVToRGB");
    auto &shaderBinder = pipelineManager_->GetShaderVariableBinder(Passkey<Renderer>{}, "VideoYUVToRGB");
    shaderBinder.SetCommandList(commandList);

    for (auto *videoTexture : pendingConversions) {
        if (!videoTexture) continue;

        auto *luma = videoTexture->GetLumaResource(Passkey<Renderer>{});
        auto *chroma = videoTexture->GetChromaResource(Passkey<Renderer>{});
        auto *rgbaUav = videoTexture->GetRgbaUavResource(Passkey<Renderer>{});
        if (!luma || !chroma || !rgbaUav) continue;

        rgbaUav->SetCommandList(commandList);
        rgbaUav->TransitionTo(D3D12_RESOURCE_STATE_UNORDERED_ACCESS);

        shaderBinder.Bind("Compute:gTextureY", luma);
        shaderBinder.Bind("Compute:gTextureUV", chroma);
        shaderBinder.Bind("Compute:gOutput", rgbaUav);

        const std::uint32_t groupX = (videoTexture->GetWidth() + 7) / 8;
        const std::uint32_t groupY = (videoTexture->GetHeight() + 7) / 8;
        commandList->Dispatch(groupX, groupY, 1);

        // 後続の描画パスでSRVとして読めるように状態遷移しておく
        rgbaUav->TransitionTo(D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE);

        videoTexture->ClearPendingConversion(Passkey<Renderer>{});
    }
}

void Renderer::ProcessSkinning(SceneContext *sceneContext) {
    LogScope scope;
    if (!sceneContext || !pipelineManager_) return;
    auto *sceneRenderer = sceneContext->GetComponent<SceneRenderer>();
    if (!sceneRenderer) return;
    const auto &renderers = sceneRenderer->GetSkinnedMeshRenderers();
    if (renderers.empty()) return;
    if (!pipelineManager_->HasPipeline("Skinning") || pipelineManager_->GetPipeline("Skinning").Type() != PipelineType::Compute) return;

    auto *commandList = ComputeCommandProcessor::GetCommandList(Passkey<Renderer>{});
    if (!commandList) return;

    // 共有コマンドリスト上でもフェーズ開始時のパイプライン状態は明示的に作り直す
    PipelineBinder pipelineBinder(commandList, pipelineManager_);
    pipelineBinder.Invalidate();

    for (auto *renderer : renderers) {
        if (!renderer || !renderer->IsActive()) continue;
        // ゲームループが停止/一時停止中でも描画自体は継続されるため、Update()に頼らずここで
        // 毎フレーム明示的にリソースを最新化する（CameraRenderer::RefreshConstantBufferと同じ考え方）
        renderer->RefreshSkinningResources(Passkey<Renderer>{});
        if (!renderer->HasValidSkinningData(Passkey<Renderer>{})) continue;

        auto *sourceVertices = renderer->GetSourceVerticesBuffer(Passkey<Renderer>{});
        auto *skinWeights = renderer->GetSkinWeightsBuffer(Passkey<Renderer>{});
        auto *boneMatrices = renderer->GetBoneMatricesBuffer(Passkey<Renderer>{});
        auto *constantBuffer = renderer->GetSkinningConstantBuffer(Passkey<Renderer>{});
        auto *outputBuffer = renderer->GetSkinnedVertexBuffer(Passkey<Renderer>{});
        auto *blendShapeDeltas = renderer->GetBlendShapeDeltasBuffer(Passkey<Renderer>{});
        auto *blendShapeWeights = renderer->GetBlendShapeWeightsBuffer(Passkey<Renderer>{});
        if (!sourceVertices || !skinWeights || !boneMatrices || !constantBuffer || !outputBuffer ||
            !blendShapeDeltas || !blendShapeWeights) continue;

        pipelineBinder.UsePipeline("Skinning");
        auto &shaderBinder = pipelineManager_->GetShaderVariableBinder(Passkey<Renderer>{}, "Skinning");
        shaderBinder.SetCommandList(commandList);

        outputBuffer->SetCommandList(commandList);
        outputBuffer->TransitionTo(D3D12_RESOURCE_STATE_UNORDERED_ACCESS);

        // cbufferのバインド名は変数名ではなくブロック名(cbuffer <名前>)で登録されるため注意
        shaderBinder.Bind("Compute:SkinningConstants", constantBuffer);
        shaderBinder.Bind("Compute:gSourceVertices", sourceVertices);
        shaderBinder.Bind("Compute:gSkinWeights", skinWeights);
        shaderBinder.Bind("Compute:gBoneMatrices", boneMatrices);
        shaderBinder.Bind("Compute:gBlendShapeDeltas", blendShapeDeltas);
        shaderBinder.Bind("Compute:gBlendShapeWeights", blendShapeWeights);
        shaderBinder.Bind("Compute:gOutputVertices", outputBuffer);

        const std::uint32_t vertexCount = renderer->GetVertexCount(Passkey<Renderer>{});
        const std::uint32_t groupCount = std::max<std::uint32_t>(1, (vertexCount + 63) / 64);
        commandList->Dispatch(groupCount, 1, 1);

        // 後続の描画パスで頂点バッファとして読めるように状態遷移しておく
        outputBuffer->TransitionTo(D3D12_RESOURCE_STATE_VERTEX_AND_CONSTANT_BUFFER);
    }

}

void Renderer::ProcessGpuParticles(SceneContext *sceneContext) {
    LogScope scope;
    // デバッグ用: どこで早期returnしているかを一度だけログに出す（GPUパーティクルが描画されない問題の調査用）
    static bool sLoggedEmittersEmpty = false;
    static bool sLoggedSpawnPipelineMissing = false;
    static bool sLoggedUpdatePipelineMissing = false;
    static bool sLoggedDispatching = false;

    if (!sceneContext || !pipelineManager_) return;
    auto *sceneRenderer = sceneContext->GetComponent<SceneRenderer>();
    if (!sceneRenderer) return;
    const auto &emitters = sceneRenderer->GetGpuParticleEmitters();
    if (emitters.empty()) {
        if (!sLoggedEmittersEmpty) {
            sLoggedEmittersEmpty = true;
            char buf[128];
            std::snprintf(buf, sizeof(buf), "(sceneRenderer=%p)", static_cast<const void *>(sceneRenderer));
            Log(Translation("engine.gpuparticle.emitters.empty") + buf, LogSeverity::Warning);
        }
        return;
    }
    if (!pipelineManager_->HasPipeline("ParticleSpawn") || pipelineManager_->GetPipeline("ParticleSpawn").Type() != PipelineType::Compute) {
        if (!sLoggedSpawnPipelineMissing) {
            sLoggedSpawnPipelineMissing = true;
            Log(Translation("engine.gpuparticle.pipeline.notfound") + "ParticleSpawn", LogSeverity::Warning);
        }
        return;
    }
    if (!pipelineManager_->HasPipeline("ParticleUpdate") || pipelineManager_->GetPipeline("ParticleUpdate").Type() != PipelineType::Compute) {
        if (!sLoggedUpdatePipelineMissing) {
            sLoggedUpdatePipelineMissing = true;
            Log(Translation("engine.gpuparticle.pipeline.notfound") + "ParticleUpdate", LogSeverity::Warning);
        }
        return;
    }
    static bool sLoggedFreeListInitPipelineMissing = false;
    if (!pipelineManager_->HasPipeline("ParticleFreeListInit") || pipelineManager_->GetPipeline("ParticleFreeListInit").Type() != PipelineType::Compute) {
        if (!sLoggedFreeListInitPipelineMissing) {
            sLoggedFreeListInitPipelineMissing = true;
            Log(Translation("engine.gpuparticle.pipeline.notfound") + "ParticleFreeListInit", LogSeverity::Warning);
        }
        return;
    }
    if (!sLoggedDispatching) {
        sLoggedDispatching = true;
        char buf[128];
        std::snprintf(buf, sizeof(buf), "%zu", emitters.size());
        Log(Translation("engine.gpuparticle.dispatching") + buf, LogSeverity::Info);
    }

    auto *commandList = ComputeCommandProcessor::GetCommandList(Passkey<Renderer>{});
    if (!commandList) return;

    PipelineBinder pipelineBinder(commandList, pipelineManager_);
    pipelineBinder.Invalidate();

    const float deltaTime = GetDeltaTime();

    for (auto *emitter : emitters) {
        if (!emitter || !emitter->IsActive()) continue;
        // このフレームにコンポーネントのUpdateが実行されていない（＝シーンがポーズ/停止中の）場合は
        // シミュレーションを進めない。ポーズ中もRenderFrame自体は毎フレーム走るため、ここでゲートしないと
        // GPUパーティクルだけが実時間で動き続けてしまう
        if (!emitter->ConsumeGpuFrameUpdated(Passkey<Renderer>{})) continue;

        auto *particleBuffer = emitter->GetGpuParticleBuffer(Passkey<Renderer>{});
        auto *instanceMatrixBuffer = emitter->GetGpuInstanceMatrixBuffer(Passkey<Renderer>{});
        auto *spawnRequestBuffer = emitter->GetGpuSpawnRequestBuffer(Passkey<Renderer>{});
        auto *spawnConstantBuffer = emitter->GetGpuSpawnConstantBuffer(Passkey<Renderer>{});
        auto *updateConstantBuffer = emitter->GetGpuUpdateConstantBuffer(Passkey<Renderer>{});
        auto *freeListBuffer = emitter->GetGpuFreeListBuffer(Passkey<Renderer>{});
        auto *freeListCounterBuffer = emitter->GetGpuFreeListCounterBuffer(Passkey<Renderer>{});
        auto *freeListInitConstantBuffer = emitter->GetGpuFreeListInitConstantBuffer(Passkey<Renderer>{});
        if (!particleBuffer || !instanceMatrixBuffer || !spawnRequestBuffer || !spawnConstantBuffer || !updateConstantBuffer
            || !freeListBuffer || !freeListCounterBuffer || !freeListInitConstantBuffer) continue;

        particleBuffer->SetCommandList(commandList);
        instanceMatrixBuffer->SetCommandList(commandList);
        freeListBuffer->SetCommandList(commandList);
        freeListCounterBuffer->SetCommandList(commandList);
        particleBuffer->TransitionTo(D3D12_RESOURCE_STATE_UNORDERED_ACCESS);
        instanceMatrixBuffer->TransitionTo(D3D12_RESOURCE_STATE_UNORDERED_ACCESS);
        freeListBuffer->TransitionTo(D3D12_RESOURCE_STATE_UNORDERED_ACCESS);
        freeListCounterBuffer->TransitionTo(D3D12_RESOURCE_STATE_UNORDERED_ACCESS);

        const std::uint32_t particleCount = emitter->GetGpuParticleCapacity(Passkey<Renderer>{});

        // バッファが新規生成・容量変更された直後は、フリーリストへ空きスロット 0..capacity-1 を積み直す
        if (particleCount > 0 && emitter->ConsumeGpuFreeListNeedsInit(Passkey<Renderer>{})) {
            pipelineBinder.UsePipeline("ParticleFreeListInit");
            auto &initShaderBinder = pipelineManager_->GetShaderVariableBinder(Passkey<Renderer>{}, "ParticleFreeListInit");
            initShaderBinder.SetCommandList(commandList);

            GPUParticleFreeListInitConstants initConstants;
            initConstants.capacity = particleCount;
            void *mappedInitConstants = freeListInitConstantBuffer->Map();
            if (mappedInitConstants) std::memcpy(mappedInitConstants, &initConstants, sizeof(initConstants));

            initShaderBinder.Bind("Compute:ParticleFreeListInitConstants", freeListInitConstantBuffer);
            initShaderBinder.Bind("Compute:gFreeList", freeListBuffer);
            initShaderBinder.Bind("Compute:gFreeListCounter", freeListCounterBuffer);

            const std::uint32_t initGroupCount = std::max<std::uint32_t>(1, (particleCount + 63) / 64);
            commandList->Dispatch(initGroupCount, 1, 1);

            // ParticleSpawnが直後にgFreeList/gFreeListCounterを読み書きするため、書き込み完了を待つ
            D3D12_RESOURCE_BARRIER initBarriers[2]{};
            initBarriers[0].Type = D3D12_RESOURCE_BARRIER_TYPE_UAV;
            initBarriers[0].UAV.pResource = freeListBuffer->GetResource();
            initBarriers[1].Type = D3D12_RESOURCE_BARRIER_TYPE_UAV;
            initBarriers[1].UAV.pResource = freeListCounterBuffer->GetResource();
            commandList->ResourceBarrier(2, initBarriers);
        }

        const std::uint32_t spawnCount = emitter->GetGpuSpawnCount(Passkey<Renderer>{});
        if (spawnCount > 0) {
            pipelineBinder.UsePipeline("ParticleSpawn");
            auto &spawnShaderBinder = pipelineManager_->GetShaderVariableBinder(Passkey<Renderer>{}, "ParticleSpawn");
            spawnShaderBinder.SetCommandList(commandList);

            GPUParticleSpawnConstants spawnConstants;
            spawnConstants.spawnCount = spawnCount;
            void *mappedSpawnConstants = spawnConstantBuffer->Map();
            if (mappedSpawnConstants) std::memcpy(mappedSpawnConstants, &spawnConstants, sizeof(spawnConstants));

            spawnShaderBinder.Bind("Compute:ParticleSpawnConstants", spawnConstantBuffer);
            spawnShaderBinder.Bind("Compute:gSpawnRequests", spawnRequestBuffer);
            spawnShaderBinder.Bind("Compute:gParticles", particleBuffer);
            spawnShaderBinder.Bind("Compute:gFreeList", freeListBuffer);
            spawnShaderBinder.Bind("Compute:gFreeListCounter", freeListCounterBuffer);

            const std::uint32_t spawnGroupCount = std::max<std::uint32_t>(1, (spawnCount + 63) / 64);
            commandList->Dispatch(spawnGroupCount, 1, 1);

            // ParticleUpdateが直後にgParticles/gFreeList/gFreeListCounterを読み書きするため、
            // 書き込み完了を待つUAVバリアを挟む
            D3D12_RESOURCE_BARRIER uavBarriers[3]{};
            uavBarriers[0].Type = D3D12_RESOURCE_BARRIER_TYPE_UAV;
            uavBarriers[0].UAV.pResource = particleBuffer->GetResource();
            uavBarriers[1].Type = D3D12_RESOURCE_BARRIER_TYPE_UAV;
            uavBarriers[1].UAV.pResource = freeListBuffer->GetResource();
            uavBarriers[2].Type = D3D12_RESOURCE_BARRIER_TYPE_UAV;
            uavBarriers[2].UAV.pResource = freeListCounterBuffer->GetResource();
            commandList->ResourceBarrier(3, uavBarriers);
        }

        if (particleCount == 0) continue;

        pipelineBinder.UsePipeline("ParticleUpdate");
        auto &updateShaderBinder = pipelineManager_->GetShaderVariableBinder(Passkey<Renderer>{}, "ParticleUpdate");
        updateShaderBinder.SetCommandList(commandList);

        GPUParticleUpdateConstants updateConstants;
        updateConstants.deltaTime = deltaTime;
        updateConstants.particleCount = particleCount;
        updateConstants.billboard = emitter->IsBillboard() ? 1u : 0u;
        updateConstants.billboardMode = static_cast<std::uint32_t>(emitter->GetBillboardRotationMode());
        if (emitter->IsBillboard()) {
            auto *cameraObject = emitter->ResolveBillboardCameraObject(Passkey<Renderer>{});
            auto *cameraTransform = cameraObject ? cameraObject->GetComponent<Transform>() : nullptr;
            if (cameraTransform) updateConstants.cameraWorldMatrix = cameraTransform->GetWorldMatrix();
        }
        // SpawnOrigin::ChildOfSelf/ChildOfOtherの場合、パーティクルは親のローカル空間で
        // シミュレーションされているため、毎フレーム親の現在のワールド行列を渡して追従させる
        // （親が無い場合はデフォルトの恒等行列のまま＝従来通りワールド空間として扱われる）
        if (auto *followParent = emitter->ResolveGpuFollowParent(Passkey<Renderer>{})) {
            auto *followParentTransform = followParent->GetComponent<Transform>();
            if (followParentTransform) updateConstants.parentWorldMatrix = followParentTransform->GetWorldMatrix();
        }
        void *mappedUpdateConstants = updateConstantBuffer->Map();
        if (mappedUpdateConstants) std::memcpy(mappedUpdateConstants, &updateConstants, sizeof(updateConstants));

        updateShaderBinder.Bind("Compute:ParticleUpdateConstants", updateConstantBuffer);
        updateShaderBinder.Bind("Compute:gParticles", particleBuffer);
        updateShaderBinder.Bind("Compute:gInstanceMatrices", instanceMatrixBuffer);
        updateShaderBinder.Bind("Compute:gFreeList", freeListBuffer);
        updateShaderBinder.Bind("Compute:gFreeListCounter", freeListCounterBuffer);

        const std::uint32_t updateGroupCount = std::max<std::uint32_t>(1, (particleCount + 63) / 64);
        commandList->Dispatch(updateGroupCount, 1, 1);

        // 後続の描画パスでSRVとして読めるように状態遷移しておく
        instanceMatrixBuffer->TransitionTo(D3D12_RESOURCE_STATE_NON_PIXEL_SHADER_RESOURCE | D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE);
    }

}


} // namespace KashipanEngine
