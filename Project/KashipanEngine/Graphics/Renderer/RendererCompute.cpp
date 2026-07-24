#include "RendererInternal.h"

namespace KashipanEngine {

using namespace RendererInternal;

void Renderer::ProcessComputeShaders(SceneContext *sceneContext) {
    if (!sceneContext) return;
    auto *sceneProcessor = sceneContext->GetComponent<SceneComputeProcessor>();
    if (!sceneProcessor) return;
    const auto &components = sceneProcessor->GetComputeShaderProcessings();
    if (components.empty()) return;

    auto *commandList = ComputeCommandProcessor::BeginRecord(Passkey<Renderer>{});
    if (!commandList) return;

    // 専用コマンドリストはフレームごとにReset()されるため、パイプラインバインド状態は毎回作り直す
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

    ComputeCommandProcessor::EndRecord(Passkey<Renderer>{});
}

void Renderer::ProcessSkinning(SceneContext *sceneContext) {
    if (!sceneContext || !pipelineManager_) return;
    auto *sceneRenderer = sceneContext->GetComponent<SceneRenderer>();
    if (!sceneRenderer) return;
    const auto &renderers = sceneRenderer->GetSkinnedMeshRenderers();
    if (renderers.empty()) return;
    if (!pipelineManager_->HasPipeline("Skinning") || pipelineManager_->GetPipeline("Skinning").Type() != PipelineType::Compute) return;

    // スキニング専用のコマンドリストへ記録する（ComputeCommandProcessorの共有コマンドリストは
    // 後続フェーズのBeginRecord内のReset()で記録内容が提出前に上書きされてしまうため使わない）
    auto *commandList = BeginDedicatedComputeCommandList(skinningCommandSlotIndex_, skinningCommands_);
    if (!commandList) return;

    // 専用コマンドリストはフレームごとにReset()されるため、パイプラインバインド状態は毎回作り直す
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

    EndDedicatedComputeCommandList(skinningCommands_);
}

void Renderer::ProcessGpuParticles(SceneContext *sceneContext) {
    // デバッグ用: どこで早期returnしているかを一度だけログに出す（GPUパーティクルが描画されない問題の調査用）
    static bool sLoggedEmittersEmpty = false;
    static bool sLoggedSpawnPipelineMissing = false;
    static bool sLoggedUpdatePipelineMissing = false;
    static bool sLoggedDispatching = false;

    if (!sceneContext || !pipelineManager_ || !directXCommon_) return;
    auto *sceneRenderer = sceneContext->GetComponent<SceneRenderer>();
    if (!sceneRenderer) return;
    const auto &emitters = sceneRenderer->GetGpuParticleEmitters();
    if (emitters.empty()) {
        if (!sLoggedEmittersEmpty) {
            sLoggedEmittersEmpty = true;
            char buf[128];
            std::snprintf(buf, sizeof(buf), "[ProcessGpuParticles] emitters is empty (sceneRenderer=%p)", static_cast<const void *>(sceneRenderer));
            Log(buf, LogSeverity::Warning);
        }
        return;
    }
    if (!pipelineManager_->HasPipeline("ParticleSpawn") || pipelineManager_->GetPipeline("ParticleSpawn").Type() != PipelineType::Compute) {
        if (!sLoggedSpawnPipelineMissing) {
            sLoggedSpawnPipelineMissing = true;
            Log("[ProcessGpuParticles] \"ParticleSpawn\" compute pipeline not found/loaded", LogSeverity::Warning);
        }
        return;
    }
    if (!pipelineManager_->HasPipeline("ParticleUpdate") || pipelineManager_->GetPipeline("ParticleUpdate").Type() != PipelineType::Compute) {
        if (!sLoggedUpdatePipelineMissing) {
            sLoggedUpdatePipelineMissing = true;
            Log("[ProcessGpuParticles] \"ParticleUpdate\" compute pipeline not found/loaded", LogSeverity::Warning);
        }
        return;
    }
    if (!sLoggedDispatching) {
        sLoggedDispatching = true;
        char buf[128];
        std::snprintf(buf, sizeof(buf), "[ProcessGpuParticles] dispatching for %zu emitter(s)", emitters.size());
        Log(buf, LogSeverity::Info);
    }

    // GPUパーティクル専用のコマンドリストを使う（ComputeCommandProcessorの共有コマンドリストは
    // 他フェーズのBeginRecord内のReset()で記録内容が提出前に上書きされてしまうため使わない）
    auto *commandList = BeginDedicatedComputeCommandList(particleComputeCommandSlotIndex_, particleComputeCommands_);
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
        if (!particleBuffer || !instanceMatrixBuffer || !spawnRequestBuffer || !spawnConstantBuffer || !updateConstantBuffer) continue;

        particleBuffer->SetCommandList(commandList);
        instanceMatrixBuffer->SetCommandList(commandList);
        particleBuffer->TransitionTo(D3D12_RESOURCE_STATE_UNORDERED_ACCESS);
        instanceMatrixBuffer->TransitionTo(D3D12_RESOURCE_STATE_UNORDERED_ACCESS);

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

            const std::uint32_t spawnGroupCount = std::max<std::uint32_t>(1, (spawnCount + 63) / 64);
            commandList->Dispatch(spawnGroupCount, 1, 1);

            // ParticleUpdateが直後にgParticlesを読み書きするため、書き込み完了を待つUAVバリアを挟む
            D3D12_RESOURCE_BARRIER uavBarrier{};
            uavBarrier.Type = D3D12_RESOURCE_BARRIER_TYPE_UAV;
            uavBarrier.UAV.pResource = particleBuffer->GetResource();
            commandList->ResourceBarrier(1, &uavBarrier);
        }

        const std::uint32_t particleCount = emitter->GetGpuParticleCapacity(Passkey<Renderer>{});
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

        const std::uint32_t updateGroupCount = std::max<std::uint32_t>(1, (particleCount + 63) / 64);
        commandList->Dispatch(updateGroupCount, 1, 1);

        // 後続の描画パスでSRVとして読めるように状態遷移しておく
        instanceMatrixBuffer->TransitionTo(D3D12_RESOURCE_STATE_NON_PIXEL_SHADER_RESOURCE | D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE);
    }

    if (particleComputeCommands_->EndRecord()) {
        directXCommon_->AddRecordCommandList(Passkey<Renderer>{}, particleComputeCommands_->GetCommandList());
    }
}


} // namespace KashipanEngine
