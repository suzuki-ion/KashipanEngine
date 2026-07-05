#include "Renderer.h"

#include <algorithm>
#include <cstring>
#include <vector>

#include "Assets/MaterialManager.h"
#include "Assets/SamplerManager.h"
#include "Assets/TextureManager.h"
#include "Core/DirectXCommon.h"
#include "Graphics/IRenderTarget.h"
#include "Graphics/Pipeline/System/PipelineBinder.h"
#include "Graphics/PipelineManager.h"
#include "Graphics/Renderer/ResourceContainer.h"
#include "Graphics/ScreenBuffer.h"
#include "Objects/Components/PostProcessing/IPostProcessComponent.h"
#include "Objects/Components/Render/CameraRenderer.h"
#include "Objects/Components/Render/LightRenderer.h"
#include "Objects/Components/Render/MeshRenderer.h"
#include "Objects/EmptyObject.h"
#include "Scene/SceneContext.h"

namespace KashipanEngine {

namespace {

/// @brief gMaterials 構造化バッファと同レイアウトの構造体
#pragma pack(push, 4)
struct MaterialElement {
    float enableLighting = 1.0f;
    float enableEnvironmentMapping = 0.0f;
    float enableShadowMapProjection = 1.0f;
    Vector4 color{ 1.0f, 1.0f, 1.0f, 1.0f };
    Matrix4x4 uvTransform = Matrix4x4::Identity();
    float shininess = 32.0f;
    Vector4 specularColor{ 1.0f, 1.0f, 1.0f, 1.0f };
    float environmentCoefficient = 0.0f;
};
#pragma pack(pop)

/// @brief バッファキャッシュキー生成（描画先＋パイプライン＋メッシュ＋マテリアルでバッチを識別）
std::string MakeBatchKey(const void *target, const std::string &pipelineName,
    std::uint32_t meshHandle, std::uint32_t materialHandle, const char *usage) {
    char buffer[64];
    std::snprintf(buffer, sizeof(buffer), "%p|%u|%u|%s|", target, meshHandle, materialHandle, usage);
    return std::string(buffer) + pipelineName;
}

} // namespace

Renderer::Renderer(Passkey<GraphicsEngine>, DirectXCommon *directXCommon, PipelineManager *pipelineManager)
    : directXCommon_(directXCommon), pipelineManager_(pipelineManager) {
    resourceContainer_ = std::make_unique<ResourceContainer>();
}

Renderer::~Renderer() = default;

void Renderer::RenderFrame(Passkey<GraphicsEngine>, SceneContext *sceneContext) {
    if (!sceneContext || !pipelineManager_) return;

    auto *sceneRenderer = sceneContext->GetComponent<SceneRenderer>();
    if (!sceneRenderer) return;

    const auto &drawList = sceneRenderer->BuildSortedDrawList(Passkey<Renderer>{}, pipelineManager_);
    if (drawList.empty()) return;

    // 描画先ごとの範囲に区切って描画（リストは描画先順でソート済み）
    size_t begin = 0;
    while (begin < drawList.size()) {
        IRenderTarget *target = drawList[begin].target;
        size_t end = begin;
        while (end < drawList.size() && drawList[end].target == target) ++end;

        RenderToTarget(target,
            std::span<const SceneRenderer::DrawEntry>(drawList.data() + begin, end - begin),
            sceneRenderer);
        begin = end;
    }
}

void Renderer::ReleaseAllResources(Passkey<GraphicsEngine>) {
    if (resourceContainer_) {
        resourceContainer_->Clear();
    }
}

void Renderer::RenderToTarget(IRenderTarget *target,
    std::span<const SceneRenderer::DrawEntry> entries,
    SceneRenderer *sceneRenderer) {
    if (!target) return;

    target->BeginDraw();
    auto *commandList = target->GetCommandList();
    if (!commandList) return;

    PipelineBinder pipelineBinder(commandList, pipelineManager_);

    // 同一（パイプライン・メッシュ・マテリアル）の連続範囲をバッチとしてまとめて描画
    size_t begin = 0;
    while (begin < entries.size()) {
        auto *renderer = entries[begin].renderer;
        size_t end = begin;
        while (end < entries.size()) {
            auto *other = entries[end].renderer;
            if (other->GetPipelineName() != renderer->GetPipelineName() ||
                other->GetMeshHandle() != renderer->GetMeshHandle() ||
                other->GetMaterialHandle() != renderer->GetMaterialHandle()) {
                break;
            }
            ++end;
        }

        DrawBatch(target, pipelineBinder, entries.subspan(begin, end - begin), sceneRenderer);
        begin = end;
    }

    // ScreenBuffer の場合は所有オブジェクトのポストプロセスを適用
    if (target->GetRenderTargetKind() == RenderTargetKind::ScreenBuffer) {
        RenderPostProcess(static_cast<ScreenBuffer *>(target), pipelineBinder, sceneRenderer);
    }

    // ウィンドウのコマンドリストはこの後 ImGui 等の描画にも使われるため、
    // 描画終了処理はスワップチェーン側（DirectXCommon::EndDraw）に任せる
    if (target->GetRenderTargetKind() != RenderTargetKind::Window) {
        target->EndDraw();
    }
}

void Renderer::DrawBatch(IRenderTarget *target,
    PipelineBinder &pipelineBinder,
    std::span<const SceneRenderer::DrawEntry> batch,
    SceneRenderer *sceneRenderer) {
    if (batch.empty()) return;

    auto *meshRenderer = batch.front().renderer;
    const std::string &pipelineName = meshRenderer->GetPipelineName();
    auto *commandList = target->GetCommandList();

    const auto *meshBuffers = resourceContainer_->GetOrCreateMeshBuffers(meshRenderer->GetMeshHandle());
    if (!meshBuffers || !meshBuffers->vertexBuffer || !meshBuffers->indexBuffer) return;

    pipelineBinder.UsePipeline(pipelineName);
    auto &shaderBinder = pipelineManager_->GetShaderVariableBinder(Passkey<Renderer>{}, pipelineName);
    shaderBinder.SetCommandList(commandList);

    // カメラ・ライトの定数バッファバインド
    BindCameraAndLights(commandList, target, pipelineName, sceneRenderer);

    const std::uint32_t instanceCount = static_cast<std::uint32_t>(batch.size());

    // ワールド行列のインスタンスバッファ
    {
        auto key = MakeBatchKey(target, pipelineName, meshRenderer->GetMeshHandle(), meshRenderer->GetMaterialHandle(), "transform");
        auto *instanceBuffer = resourceContainer_->GetOrCreateStructuredBuffer(key, sizeof(Matrix4x4), instanceCount);
        if (!instanceBuffer) return;
        auto *mapped = static_cast<Matrix4x4 *>(instanceBuffer->Map());
        if (!mapped) return;
        for (size_t i = 0; i < batch.size(); ++i) {
            mapped[i] = batch[i].renderer->GetWorldMatrix();
        }
        shaderBinder.Bind("Vertex:gTransformationMatrices", instanceBuffer);
    }

    // マテリアルの構造化バッファ（シェーダーはインスタンスIDで参照するため個数分並べる）
    {
        MaterialElement element;
        const auto *material = MaterialManager::GetMaterial(meshRenderer->GetMaterialHandle());
        if (material) {
            element.enableLighting = material->enableLighting ? 1.0f : 0.0f;
            element.enableEnvironmentMapping = (material->environmentHandle != TextureManager::kInvalidHandle) ? 1.0f : 0.0f;
            element.enableShadowMapProjection = material->enableShadowMapProjection ? 1.0f : 0.0f;
            element.color = material->color;
            element.uvTransform = material->uvTransform;
            element.shininess = material->shininess;
            element.specularColor = material->specularColor;
            element.environmentCoefficient = material->environmentCoefficient;
        }

        auto key = MakeBatchKey(target, pipelineName, meshRenderer->GetMeshHandle(), meshRenderer->GetMaterialHandle(), "material");
        auto *materialBuffer = resourceContainer_->GetOrCreateStructuredBuffer(key, sizeof(MaterialElement), instanceCount);
        if (materialBuffer) {
            auto *mapped = static_cast<MaterialElement *>(materialBuffer->Map());
            if (mapped) {
                for (std::uint32_t i = 0; i < instanceCount; ++i) {
                    mapped[i] = element;
                }
                shaderBinder.Bind("Pixel:gMaterials", materialBuffer);
            }
        }

        // マテリアルのテクスチャ・サンプラーバインド
        if (material && material->textureHandle != TextureManager::kInvalidHandle) {
            TextureManager::BindTexture(&shaderBinder, "Pixel:gTexture", material->textureHandle);
        }
        if (material && material->environmentHandle != TextureManager::kInvalidHandle) {
            TextureManager::BindTexture(&shaderBinder, "Pixel:gEnvironmentMap", material->environmentHandle);
        }
        if (material && material->samplerHandle != SamplerManager::kInvalidHandle) {
            SamplerManager::BindSampler(&shaderBinder, "Pixel:gSampler", material->samplerHandle);
        }
    }

    // メッシュのバインドと描画
    pipelineBinder.SetVertexBuffer(meshBuffers->vertexBuffer.get(), sizeof(ResourceContainer::MeshVertex));
    pipelineBinder.SetIndexBuffer(meshBuffers->indexBuffer.get());
    commandList->DrawIndexedInstanced(meshBuffers->indexCount, instanceCount, 0, 0, 0);
}

namespace {

/// @brief 指定の描画先が対象オブジェクトの描画先に含まれるか（未指定の場合は全描画先に適用）
bool IsTargetMatch(EmptyObject *targetObject, bool hasTargetSpecified, IRenderTarget *target) {
    if (!hasTargetSpecified) return true;
    if (!targetObject) return false; // 指定されているが解決できない場合は適用しない
    std::vector<IRenderTarget *> targets;
    SceneRenderer::CollectRenderTargets(targetObject, targets);
    return std::find(targets.begin(), targets.end(), target) != targets.end();
}

} // namespace

void Renderer::BindCameraAndLights(ID3D12GraphicsCommandList *commandList,
    IRenderTarget *target,
    const std::string &pipelineName,
    SceneRenderer *sceneRenderer) {
    auto &shaderBinder = pipelineManager_->GetShaderVariableBinder(Passkey<Renderer>{}, pipelineName);
    shaderBinder.SetCommandList(commandList);

    for (auto *cameraRenderer : sceneRenderer->GetCameraRenderers()) {
        if (!cameraRenderer || !cameraRenderer->IsActive()) continue;
        // パイプライン指定がある場合は一致するパイプラインのみバインド
        if (!cameraRenderer->GetPipelineName().empty() && cameraRenderer->GetPipelineName() != pipelineName) continue;
        // 描画先指定がある場合は一致する描画先のみバインド
        if (!IsTargetMatch(cameraRenderer->GetTargetObject(), cameraRenderer->GetTargetObjectID().IsValid(), target)) continue;
        auto *constantBuffer = cameraRenderer->GetConstantBuffer();
        if (!constantBuffer) continue;
        for (const auto &variableName : cameraRenderer->GetBindVariableNames()) {
            shaderBinder.Bind(variableName, constantBuffer);
        }
    }

    for (auto *lightRenderer : sceneRenderer->GetLightRenderers()) {
        if (!lightRenderer || !lightRenderer->IsActive()) continue;
        if (!lightRenderer->GetPipelineName().empty() && lightRenderer->GetPipelineName() != pipelineName) continue;
        if (!IsTargetMatch(lightRenderer->GetTargetObject(), lightRenderer->GetTargetObjectID().IsValid(), target)) continue;
        auto *constantBuffer = lightRenderer->GetConstantBuffer();
        if (!constantBuffer) continue;
        for (const auto &variableName : lightRenderer->GetBindVariableNames()) {
            shaderBinder.Bind(variableName, constantBuffer);
        }
    }
}

void Renderer::RenderPostProcess(ScreenBuffer *screenBuffer,
    PipelineBinder &pipelineBinder,
    SceneRenderer *sceneRenderer) {
    if (!screenBuffer || !sceneRenderer) return;

    auto *ownerObject = sceneRenderer->GetTargetOwner(screenBuffer);
    if (!ownerObject) return;

    auto postProcessComponents = ownerObject->GetComponents<IPostProcessComponent>();
    if (postProcessComponents.empty()) return;

    auto *commandList = screenBuffer->GetCommandList();
    if (!commandList) return;

    for (auto *component : postProcessComponents) {
        if (!component || !component->IsActive()) continue;

        auto passes = component->BuildPassesInterface(Passkey<Renderer>{});
        for (const auto &pass : passes) {
            if (pass.pipelineName.empty() || !pipelineManager_->HasPipeline(pass.pipelineName)) continue;

            // 直前パスの結果をSRVとして参照可能にし、次の書き込み面へ切り替える
            screenBuffer->NextPass();

            pipelineBinder.UsePipeline(pass.pipelineName);
            auto &shaderBinder = pipelineManager_->GetShaderVariableBinder(Passkey<Renderer>{}, pass.pipelineName);
            shaderBinder.SetCommandList(commandList);

            // 直前パスの描画結果と既定サンプラー
            shaderBinder.Bind("Pixel:gTexture", screenBuffer->GetSrvHandle());
            SamplerManager::BindSampler(&shaderBinder, "Pixel:gSampler", DefaultSampler::LinearClamp);

            // コンポーネントが要求する追加テクスチャ
            for (const auto &requirement : pass.textureBindRequirements) {
                if (!requirement.getHandle) continue;
                const auto handle = requirement.getHandle();
                if (handle.ptr == 0) continue;
                shaderBinder.Bind(requirement.variableName, handle);
            }

            // コンポーネントが要求するサンプラー
            for (const auto &requirement : pass.samplerBindRequirements) {
                SamplerManager::BindSampler(&shaderBinder, requirement.variableName, requirement.sampler);
            }

            // コンポーネントが要求する定数バッファ
            for (const auto &requirement : pass.constantBufferRequirements) {
                if (requirement.byteSize == 0 || !requirement.dataPtr) continue;
                const std::string key = MakeBatchKey(component, pass.pipelineName, 0, 0, requirement.variableName.c_str());
                auto *constantBuffer = resourceContainer_->GetOrCreateConstantBuffer(key, requirement.byteSize);
                if (!constantBuffer) continue;
                void *mapped = constantBuffer->Map();
                if (!mapped) continue;
                std::memcpy(mapped, requirement.dataPtr, requirement.byteSize);
                shaderBinder.Bind(requirement.variableName, constantBuffer);
            }

            // コンポーネントが要求するインスタンスバッファ（要素数1固定）
            for (const auto &requirement : pass.instanceBufferRequirements) {
                if (requirement.elementStride == 0 || !requirement.dataPtr) continue;
                const std::string key = MakeBatchKey(component, pass.pipelineName, 0, 1, requirement.variableName.c_str());
                auto *instanceBuffer = resourceContainer_->GetOrCreateStructuredBuffer(key, requirement.elementStride, 1);
                if (!instanceBuffer) continue;
                void *mapped = instanceBuffer->Map();
                if (!mapped) continue;
                std::memcpy(mapped, requirement.dataPtr, requirement.elementStride);
                shaderBinder.Bind(requirement.variableName, instanceBuffer);
            }

            // フルスクリーン三角形描画
            commandList->DrawInstanced(3, 1, 0, 0);
        }
    }
}

} // namespace KashipanEngine
