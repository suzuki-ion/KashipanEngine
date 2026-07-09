#include "Renderer.h"

#include <algorithm>
#include <cstring>
#include <vector>

#include "Assets/MaterialManager.h"
#include "Assets/SamplerManager.h"
#include "Assets/TextureManager.h"
#include "Core/DirectXCommon.h"
#include "Graphics/ComputeCommandProcessor.h"
#include "Graphics/IRenderTarget.h"
#include "Graphics/Pipeline/System/PipelineBinder.h"
#include "Graphics/PipelineManager.h"
#include "Graphics/Renderer/ResourceContainer.h"
#include "Graphics/Resources/ConstantBufferResource.h"
#include "Graphics/ScreenBuffer.h"
#include "Graphics/ShadowMapBuffer.h"
#include "Math/Vector3.h"
#include "Objects/Components/Compute/ComputeShaderProcessing.h"
#include "Objects/Components/PostProcessing/IPostProcessComponent.h"
#include "Objects/Components/Render/CameraRenderer.h"
#include "Objects/Components/Render/Light.h"
#include "Objects/Components/Render/LightRenderer.h"
#include "Objects/Components/Render/MeshRenderer.h"
#include "Objects/Components/Render/SkinnedMeshRenderer.h"
#include "Objects/Components/Render/ShadowMapObject.h"
#include "Graphics/Resources/RWStructuredBufferResource.h"
#include "Objects/EmptyObject.h"
#include "Scene/Components/Compute/SceneComputeProcessor.h"
#include "Scene/SceneContext.h"

namespace KashipanEngine {

namespace {

/// @brief gMaterials 構造化バッファ（Object3D）と同レイアウトの構造体
#pragma pack(push, 4)
struct MaterialElement {
    float enableLighting = 1.0f;
    float enableEnvironmentMapping = 0.0f;
    float enableShadowMapProjection = 1.0f;
    float useTexture = 1.0f;
    Vector4 color{ 1.0f, 1.0f, 1.0f, 1.0f };
    Matrix4x4 uvTransform = Matrix4x4::Identity();
    float shininess = 32.0f;
    Vector4 specularColor{ 1.0f, 1.0f, 1.0f, 1.0f };
    float environmentCoefficient = 0.0f;
};

/// @brief gMaterials 構造化バッファ（Object2D）と同レイアウトの構造体
struct Material2DElement {
    Vector4 color{ 1.0f, 1.0f, 1.0f, 1.0f };
    Matrix4x4 uvTransform = Matrix4x4::Identity();
    float useTexture = 1.0f;
    float padding[3]{};
};

/// @brief gPointLights 構造化バッファと同レイアウトの構造体
struct PointLightElement {
    std::uint32_t enabled = 0;
    Vector4 color{ 1.0f, 1.0f, 1.0f, 1.0f };
    Vector3 position{ 0.0f, 0.0f, 0.0f };
    float radius = 10.0f;
    float intensity = 1.0f;
    float decay = 2.0f;
};

/// @brief gSpotLights 構造化バッファと同レイアウトの構造体
struct SpotLightElement {
    std::uint32_t enabled = 0;
    Vector4 color{ 1.0f, 1.0f, 1.0f, 1.0f };
    Vector3 position{ 0.0f, 0.0f, 0.0f };
    float distance = 10.0f;
    Vector3 direction{ 0.0f, -1.0f, 0.0f };
    float innerAngle = 0.35f;
    float outerAngle = 0.6f;
    float intensity = 1.0f;
    float decay = 2.0f;
};

/// @brief gDirectionalLights 構造化バッファと同レイアウトの構造体
struct DirectionalLightElement {
    std::uint32_t enabled = 0;
    Vector4 color{ 1.0f, 1.0f, 1.0f, 1.0f };
    Vector3 direction{ 0.0f, -1.0f, 0.0f };
    float intensity = 1.0f;
};
#pragma pack(pop)

/// @brief LightCounts 定数バッファと同レイアウトの構造体
struct LightCountsData {
    std::uint32_t pointLightCount = 0;
    std::uint32_t spotLightCount = 0;
    std::uint32_t directionalLightCount = 0;
    std::uint32_t padding = 0;
};

/// @brief ShadowMapConstants 定数バッファと同レイアウトの構造体
struct ShadowMapConstantsData {
    Matrix4x4 lightViewProjection;
    float lightNear = 0.1f;
    float lightFar = 1000.0f;
    float padding[2]{};
};

/// @brief シャドウマップ比較用サンプラーのハンドルを取得する（初回に作成）
SamplerManager::SamplerHandle GetShadowSamplerCmpHandle() {
    static SamplerManager::SamplerHandle sHandle = [] {
        D3D12_SAMPLER_DESC desc{};
        desc.Filter = D3D12_FILTER_COMPARISON_MIN_MAG_MIP_LINEAR;
        desc.AddressU = D3D12_TEXTURE_ADDRESS_MODE_CLAMP;
        desc.AddressV = D3D12_TEXTURE_ADDRESS_MODE_CLAMP;
        desc.AddressW = D3D12_TEXTURE_ADDRESS_MODE_CLAMP;
        desc.ComparisonFunc = D3D12_COMPARISON_FUNC_LESS_EQUAL;
        desc.MinLOD = 0.0f;
        desc.MaxLOD = D3D12_FLOAT32_MAX;
        desc.MipLODBias = 0.0f;
        desc.MaxAnisotropy = 1;
        return SamplerManager::CreateSampler(desc);
    }();
    return sHandle;
}

/// @brief バッファキャッシュキー生成（描画先＋パイプライン＋メッシュ＋マテリアルでバッチを識別）
std::string MakeBatchKey(const void *target, const std::string &pipelineName,
    std::uint32_t meshHandle, std::uint32_t materialHandle, const char *usage) {
    char buffer[64];
    std::snprintf(buffer, sizeof(buffer), "%p|%u|%u|%s|", target, meshHandle, materialHandle, usage);
    return std::string(buffer) + pipelineName;
}

/// @brief ComputeShaderProcessing::UAVTextureBindRequirement::formatKind から DXGI_FORMAT へ変換
DXGI_FORMAT UAVFormatFromKind(int formatKind) {
    switch (formatKind) {
        case 1: return DXGI_FORMAT_R32_FLOAT;
        case 2: return DXGI_FORMAT_R32G32B32A32_FLOAT;
        default: return DXGI_FORMAT_R8G8B8A8_UNORM;
    }
}

} // namespace

Renderer::Renderer(Passkey<GraphicsEngine>, DirectXCommon *directXCommon, PipelineManager *pipelineManager)
    : directXCommon_(directXCommon), pipelineManager_(pipelineManager) {
    resourceContainer_ = std::make_unique<ResourceContainer>();
}

Renderer::~Renderer() = default;

void Renderer::RenderFrame(Passkey<GraphicsEngine>, SceneContext *sceneContext) {
    if (!sceneContext || !pipelineManager_) return;

    // Computeシェーダー処理は他の描画パスより先に実行し、結果を後続パスから参照できるようにする
    ProcessComputeShaders(sceneContext);
    // GPUスキニングも描画リスト構築より先に実行し、スキニング結果を描画パスから参照できるようにする
    ProcessSkinning(sceneContext);

    auto *sceneRenderer = sceneContext->GetComponent<SceneRenderer>();
    if (!sceneRenderer) return;

    // カメラの定数バッファは常に最新のTransformを反映する（ゲームループが停止/一時停止中でも
    // 描画自体は継続されるため、Update() 頼みだと停止中はカメラが固まって描画が崩れてしまう）
    for (auto *cameraRenderer : sceneRenderer->GetCameraRenderers()) {
        if (cameraRenderer && cameraRenderer->IsActive()) {
            cameraRenderer->RefreshConstantBuffer();
        }
    }

    const auto &drawList = sceneRenderer->BuildSortedDrawList(Passkey<Renderer>{}, pipelineManager_);

    // 描画先ごとの範囲に区切って描画（リストは描画先順でソート済み）
    std::unordered_set<const IRenderTarget *> renderedTargets;
    size_t begin = 0;
    while (begin < drawList.size()) {
        IRenderTarget *target = drawList[begin].target;
        size_t end = begin;
        while (end < drawList.size() && drawList[end].target == target) ++end;

        RenderToTarget(target,
            std::span<const SceneRenderer::DrawEntry>(drawList.data() + begin, end - begin),
            sceneRenderer);
        renderedTargets.insert(target);
        begin = end;
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

    auto *commandList = ComputeCommandProcessor::BeginRecord(Passkey<Renderer>{});
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

    ComputeCommandProcessor::EndRecord(Passkey<Renderer>{});
}

void Renderer::ReleaseAllResources(Passkey<GraphicsEngine>) {
    if (resourceContainer_) {
        resourceContainer_->Clear();
    }
}

void Renderer::RenderPostProcessOnlyTargets(SceneContext *sceneContext,
    const std::unordered_set<const IRenderTarget *> &renderedTargets) {
    for (const auto &object : sceneContext->GetSceneObjects()) {
        if (!object || !object->IsActive()) continue;

        // アクティブなポストエフェクトコンポーネントを持つオブジェクトのみ対象
        bool hasActivePostProcess = false;
        for (auto *component : object->GetComponents<IPostProcessComponent>()) {
            if (component && component->IsActive()) {
                hasActivePostProcess = true;
                break;
            }
        }
        if (!hasActivePostProcess) continue;

        for (auto *screenBufferObject : object->GetComponents<ScreenBufferObject>()) {
            if (!screenBufferObject || !screenBufferObject->IsActive()) continue;
            auto *buffer = screenBufferObject->GetScreenBuffer();
            if (!buffer || !ScreenBuffer::IsExist(buffer)) continue;
            if (!buffer->IsRenderTargetAvailable()) continue;
            // 描画リスト経由で既に描画済みの場合はポストエフェクトも適用済み
            if (renderedTargets.find(buffer) != renderedTargets.end()) continue;

            buffer->BeginDraw();
            auto *commandList = buffer->GetCommandList();
            if (!commandList) continue;
            PipelineBinder pipelineBinder(commandList, pipelineManager_);
            RenderPostProcess(buffer, pipelineBinder, object.get());
            buffer->EndDraw();
        }
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

    // エディター用描画先の場合、他の描画より先に背景（単色 or テクスチャ）を描画する
    if (target->GetRenderTargetKind() == RenderTargetKind::ScreenBuffer) {
        RenderEditorBackground(static_cast<ScreenBuffer *>(target), pipelineBinder, sceneRenderer);
    }

    // 同一（パイプライン・メッシュ・マテリアル）の連続範囲をバッチとしてまとめて描画
    size_t begin = 0;
    while (begin < entries.size()) {
        const auto &first = entries[begin];
        size_t end = begin;
        while (end < entries.size()) {
            const auto &other = entries[end];
            if (other.pipelineName != first.pipelineName ||
                other.meshHandle != first.meshHandle ||
                other.materialHandle != first.materialHandle ||
                other.skinnedVertexBuffer != first.skinnedVertexBuffer) {
                break;
            }
            ++end;
        }

        DrawBatch(target, pipelineBinder, entries.subspan(begin, end - begin), sceneRenderer);
        begin = end;
    }

    // ScreenBuffer の場合は所有オブジェクトのポストプロセスを適用
    if (target->GetRenderTargetKind() == RenderTargetKind::ScreenBuffer) {
        auto *screenBuffer = static_cast<ScreenBuffer *>(target);
        // エディター用描画先の場合、デバッグ表示（グリッド・当たり判定）をポストプロセスより先に描画する
        RenderEditorDebugOverlay(screenBuffer, pipelineBinder, sceneRenderer);
        RenderPostProcess(screenBuffer, pipelineBinder, sceneRenderer->GetTargetOwner(target));
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

    const auto &first = batch.front();
    const std::string &pipelineName = first.pipelineName;
    // Object2D系パイプラインかどうか（マテリアル構造体のレイアウトが異なる）
    const bool isObject2D = pipelineName.rfind("Object2D", 0) == 0;
    auto *commandList = target->GetCommandList();

    const auto *meshBuffers = resourceContainer_->GetOrCreateMeshBuffers(first.meshHandle);
    if (!meshBuffers || !meshBuffers->indexBuffer) return;
    // SkinnedMeshRendererから作られたエントリの場合は静的な頂点バッファではなく
    // GPUスキニング結果（インスタンス専用）を頂点バッファとして使用する
    if (!first.skinnedVertexBuffer && !meshBuffers->vertexBuffer) return;

    pipelineBinder.UsePipeline(pipelineName);
    auto &shaderBinder = pipelineManager_->GetShaderVariableBinder(Passkey<Renderer>{}, pipelineName);
    shaderBinder.SetCommandList(commandList);

    // カメラ・ライトの定数バッファバインド
    BindCameraAndLights(commandList, target, pipelineName, sceneRenderer);

    const std::uint32_t instanceCount = static_cast<std::uint32_t>(batch.size());

    // ワールド行列のインスタンスバッファ
    {
        auto key = MakeBatchKey(target, pipelineName, first.meshHandle, first.materialHandle, "transform");
        if (first.skinnedVertexBuffer) {
            // SkinnedMeshRendererのエントリはインスタンス結合されず必ずinstanceCount=1で
            // 個別にDrawBatchが呼ばれるが、同じメッシュ/マテリアル/パイプライン/描画先を
            // 参照する別インスタンスがあると上記キーが完全に一致してしまい、
            // resourceContainer_にキャッシュされた同一GPUバッファを取り合って上書きし合う
            // （結果、全インスタンスが最後に書き込まれた1つのワールド行列を参照して
            // 同じ位置に描画されてしまう）。インスタンス固有のスキニング出力バッファの
            // ポインタをキーに含めることで、インスタンスごとに専用の変換行列バッファを使う。
            char suffix[32];
            std::snprintf(suffix, sizeof(suffix), "|%p", static_cast<void *>(first.skinnedVertexBuffer));
            key += suffix;
        }
        auto *instanceBuffer = resourceContainer_->GetOrCreateStructuredBuffer(key, sizeof(Matrix4x4), instanceCount);
        if (!instanceBuffer) return;
        auto *mapped = static_cast<Matrix4x4 *>(instanceBuffer->Map());
        if (!mapped) return;
        for (size_t i = 0; i < batch.size(); ++i) {
            mapped[i] = batch[i].worldMatrix;
        }
        shaderBinder.Bind("Vertex:gTransformationMatrices", instanceBuffer);
    }

    // マテリアルの構造化バッファ（シェーダーはインスタンスIDで参照するため個数分並べる）
    {
        auto *material = MaterialManager::GetMaterial(first.materialHandle);
        if (material) {
            // 読み込み時に未解決だったテクスチャハンドルの解決を試みる
            material->ResolveTextureHandles();
        }

        auto key = MakeBatchKey(target, pipelineName, first.meshHandle, first.materialHandle, "material");
        if (first.skinnedVertexBuffer) {
            // 上記の変換行列バッファと同じ理由で、スキニングインスタンスごとに専用バッファを使う
            char suffix[32];
            std::snprintf(suffix, sizeof(suffix), "|%p", static_cast<void *>(first.skinnedVertexBuffer));
            key += suffix;
        }
        if (isObject2D) {
            Material2DElement element;
            if (material) {
                element.color = material->color;
                element.uvTransform = material->uvTransform;
                element.useTexture = (material->textureHandle != TextureManager::kInvalidHandle) ? 1.0f : 0.0f;
            }
            auto *materialBuffer = resourceContainer_->GetOrCreateStructuredBuffer(key, sizeof(Material2DElement), instanceCount);
            if (materialBuffer) {
                auto *mapped = static_cast<Material2DElement *>(materialBuffer->Map());
                if (mapped) {
                    for (std::uint32_t i = 0; i < instanceCount; ++i) {
                        mapped[i] = element;
                    }
                    shaderBinder.Bind("Pixel:gMaterials", materialBuffer);
                }
            }
        } else {
            MaterialElement element;
            if (material) {
                element.enableLighting = material->enableLighting ? 1.0f : 0.0f;
                element.enableEnvironmentMapping = (material->environmentHandle != TextureManager::kInvalidHandle) ? 1.0f : 0.0f;
                element.enableShadowMapProjection = material->enableShadowMapProjection ? 1.0f : 0.0f;
                element.useTexture = (material->textureHandle != TextureManager::kInvalidHandle) ? 1.0f : 0.0f;
                element.color = material->color;
                element.uvTransform = material->uvTransform;
                element.shininess = material->shininess;
                element.specularColor = material->specularColor;
                element.environmentCoefficient = material->environmentCoefficient;
            }
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
        }

        // マテリアルのテクスチャ・サンプラーバインド（未設定の場合は既定値をバインドする）
        if (material && material->textureHandle != TextureManager::kInvalidHandle) {
            TextureManager::BindTexture(&shaderBinder, "Pixel:gTexture", material->textureHandle);
        } else {
            const auto fallbackHandle = TextureManager::GetTextureFromFileName("white1x1.png");
            if (fallbackHandle != TextureManager::kInvalidHandle) {
                TextureManager::BindTexture(&shaderBinder, "Pixel:gTexture", fallbackHandle);
            }
        }
        if (material && material->environmentHandle != TextureManager::kInvalidHandle) {
            TextureManager::BindTexture(&shaderBinder, "Pixel:gEnvironmentMap", material->environmentHandle);
        }
        if (material && material->samplerHandle != SamplerManager::kInvalidHandle) {
            SamplerManager::BindSampler(&shaderBinder, "Pixel:gSampler", material->samplerHandle);
        } else {
            SamplerManager::BindSampler(&shaderBinder, "Pixel:gSampler", DefaultSampler::LinearWrap);
        }
    }

    // メッシュのバインドと描画
    if (first.skinnedVertexBuffer) {
        first.skinnedVertexBuffer->SetCommandList(commandList);
        D3D12_VERTEX_BUFFER_VIEW skinnedView = first.skinnedVertexBuffer->GetView(sizeof(ResourceContainer::MeshVertex));
        pipelineBinder.SetVertexBufferView(0, 1, &skinnedView);
    } else {
        pipelineBinder.SetVertexBuffer(meshBuffers->vertexBuffer.get(), sizeof(ResourceContainer::MeshVertex));
    }
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

    // エディター用描画先の場合はエディターカメラを優先してバインドする
    if (auto *editorCameraBuffer = sceneRenderer->GetEditorCameraBuffer(target)) {
        shaderBinder.Bind("Vertex:gCamera3D", editorCameraBuffer);
        shaderBinder.Bind("Pixel:gCamera3D", editorCameraBuffer);
    } else
    for (auto *cameraRenderer : sceneRenderer->GetCameraRenderers()) {
        if (!cameraRenderer || !cameraRenderer->IsActive()) continue;
        // パイプライン指定がある場合は一致するパイプラインのみバインド
        if (!cameraRenderer->GetPipelineName().empty() && cameraRenderer->GetPipelineName() != pipelineName) continue;
        // 描画先指定がある場合は一致する描画先のみバインド
        if (!IsTargetMatch(cameraRenderer->GetTargetObject(), cameraRenderer->GetTargetObjectID().IsValid(), target)) continue;
        // 除外設定されている描画先にはバインドしない
        if (!cameraRenderer->IsRenderTargetIncluded(target)) continue;
        auto *constantBuffer = cameraRenderer->GetConstantBuffer();
        if (!constantBuffer) continue;
        for (const auto &variableName : cameraRenderer->GetBindVariableNames()) {
            shaderBinder.Bind(variableName, constantBuffer);
        }
    }

    // ライトは種類ごとに構造化バッファへまとめてバインドする
    BindLightBuffersAndShadowMap(target, pipelineName, sceneRenderer, shaderBinder);
}

void Renderer::BindLightBuffersAndShadowMap(IRenderTarget *target,
    const std::string &pipelineName,
    SceneRenderer *sceneRenderer,
    ShaderVariableBinder &shaderBinder) {
    //--------- ライトの収集（種類ごとに構造化バッファへまとめる） ---------//
    std::vector<PointLightElement> pointLights;
    std::vector<SpotLightElement> spotLights;
    std::vector<DirectionalLightElement> directionalLights;
    for (auto *lightRenderer : sceneRenderer->GetLightRenderers()) {
        if (!lightRenderer || !lightRenderer->IsActive()) continue;
        if (!lightRenderer->GetPipelineName().empty() && lightRenderer->GetPipelineName() != pipelineName) continue;
        if (!IsTargetMatch(lightRenderer->GetTargetObject(), lightRenderer->GetTargetObjectID().IsValid(), target)) continue;
        if (!lightRenderer->IsRenderTargetIncluded(target)) continue;
        auto *light = lightRenderer->GetLight();
        if (!light) {
            // Light コンポーネントが無い場合は既定値のディレクショナルライトとして扱う
            DirectionalLightElement element;
            element.enabled = 1u;
            element.direction = lightRenderer->GetWorldDirection();
            directionalLights.push_back(element);
            continue;
        }

        if (light->GetType() == Light::Type::Directional) {
            DirectionalLightElement element;
            element.enabled = light->IsActive() ? 1u : 0u;
            element.color = light->GetColor();
            element.direction = lightRenderer->GetWorldDirection();
            element.intensity = light->GetIntensity();
            directionalLights.push_back(element);
        } else if (light->GetType() == Light::Type::Point) {
            PointLightElement element;
            element.enabled = light->IsActive() ? 1u : 0u;
            element.color = light->GetColor();
            element.position = lightRenderer->GetWorldPosition();
            element.radius = light->GetRadius();
            element.intensity = light->GetIntensity();
            element.decay = light->GetDecay();
            pointLights.push_back(element);
        } else if (light->GetType() == Light::Type::Spot) {
            SpotLightElement element;
            element.enabled = light->IsActive() ? 1u : 0u;
            element.color = light->GetColor();
            element.position = lightRenderer->GetWorldPosition();
            element.distance = light->GetDistance();
            element.direction = lightRenderer->GetWorldDirection();
            element.innerAngle = light->GetInnerAngle();
            element.outerAngle = light->GetOuterAngle();
            element.intensity = light->GetIntensity();
            element.decay = light->GetDecay();
            spotLights.push_back(element);
        }
    }

    //--------- ライト個数の定数バッファ ---------//
    {
        LightCountsData counts;
        counts.pointLightCount = static_cast<std::uint32_t>(pointLights.size());
        counts.spotLightCount = static_cast<std::uint32_t>(spotLights.size());
        counts.directionalLightCount = static_cast<std::uint32_t>(directionalLights.size());
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

    //--------- シャドウマップ ---------//
    // シャドウマップ描画先を持つオブジェクトを対象にしたカメラをライトカメラとして扱う
    ShadowMapBuffer *shadowMapBuffer = nullptr;
    CameraRenderer *shadowCamera = nullptr;
    for (auto *cameraRenderer : sceneRenderer->GetCameraRenderers()) {
        if (!cameraRenderer || !cameraRenderer->IsActive()) continue;
        auto *targetObject = cameraRenderer->GetTargetObject();
        if (!targetObject) continue;
        auto *shadowMapObject = targetObject->GetComponent<ShadowMapObject>();
        if (!shadowMapObject) continue;
        auto *buffer = shadowMapObject->GetShadowMapBuffer();
        if (!buffer || !ShadowMapBuffer::IsExist(buffer)) continue;
        shadowMapBuffer = buffer;
        shadowCamera = cameraRenderer;
        break;
    }

    ShadowMapConstantsData shadowConstants;
    if (shadowMapBuffer && shadowCamera) {
        shadowConstants.lightViewProjection = shadowCamera->GetViewProjectionMatrix();
        shadowConstants.lightNear = shadowCamera->GetNearClip();
        shadowConstants.lightFar = shadowCamera->GetFarClip();
        const auto srvHandle = shadowMapBuffer->GetSrvHandle();
        if (srvHandle.ptr != 0) {
            shaderBinder.Bind("Pixel:gShadowMap", srvHandle);
        }
    } else {
        // シャドウマップが無い場合は常に範囲外となる行列を渡して影を無効化する
        // （mul(worldPos, VP) が (0,0,2,1) となり ndc.z=2 で範囲外判定になる）
        std::memset(&shadowConstants.lightViewProjection, 0, sizeof(Matrix4x4));
        shadowConstants.lightViewProjection.m[3][2] = 2.0f;
        shadowConstants.lightViewProjection.m[3][3] = 1.0f;
        shadowConstants.lightNear = 0.0f;
        shadowConstants.lightFar = 1.0f;
        // 未バインドのデスクリプタアクセスを避けるためダミーテクスチャをバインドする
        const auto fallbackHandle = TextureManager::GetTextureFromFileName("white1x1.png");
        if (fallbackHandle != TextureManager::kInvalidHandle) {
            TextureManager::BindTexture(&shaderBinder, "Pixel:gShadowMap", fallbackHandle);
        }
    }

    {
        auto key = MakeBatchKey(target, pipelineName, 0, 0, "shadowMapConstants");
        auto *constantsBuffer = resourceContainer_->GetOrCreateConstantBuffer(key, sizeof(ShadowMapConstantsData));
        if (constantsBuffer) {
            if (auto *mapped = constantsBuffer->Map()) {
                std::memcpy(mapped, &shadowConstants, sizeof(shadowConstants));
                shaderBinder.Bind("Pixel:ShadowMapConstants", constantsBuffer);
            }
        }
    }
    SamplerManager::BindSampler(&shaderBinder, "Pixel:gShadowSamplerCmp", GetShadowSamplerCmpHandle());
}

void Renderer::RenderEditorDebugOverlay(ScreenBuffer *screenBuffer,
    PipelineBinder &pipelineBinder,
    SceneRenderer *sceneRenderer) {
    if (!screenBuffer || !sceneRenderer) return;

    // エディター用描画先でなければ何もしない（通常の描画先にはデバッグ表示を出さない）
    auto *cameraBuffer = sceneRenderer->GetEditorCameraBuffer(screenBuffer);
    if (!cameraBuffer) return;

    const auto &settings = sceneRenderer->GetEditorDebugDraw();
    auto *commandList = screenBuffer->GetCommandList();
    if (!commandList) return;

    if (settings.showGrid && pipelineManager_->HasPipeline("DebugGrid")) {
        struct GridConstant {
            float fadeDistance = 100.0f;
            float padding[3]{};
        };
        GridConstant gridConstant{};
        gridConstant.fadeDistance = std::max(1.0f, settings.gridFadeDistance);

        auto *gridBuffer = resourceContainer_->GetOrCreateConstantBuffer("EditorDebugGrid", sizeof(GridConstant));
        if (gridBuffer) {
            if (auto *mapped = gridBuffer->Map()) {
                std::memcpy(mapped, &gridConstant, sizeof(gridConstant));
            }

            pipelineBinder.UsePipeline("DebugGrid");
            auto &shaderBinder = pipelineManager_->GetShaderVariableBinder(Passkey<Renderer>{}, "DebugGrid");
            shaderBinder.SetCommandList(commandList);
            shaderBinder.Bind("Vertex:gCamera3D", cameraBuffer);
            shaderBinder.Bind("Pixel:gCamera3D", cameraBuffer);
            shaderBinder.Bind("Vertex:GridCB", gridBuffer);
            shaderBinder.Bind("Pixel:GridCB", gridBuffer);
            // カメラ位置を中心とした大きな板ポリゴン（2三角形）を全画面ではなくワールド空間へ直接描画する
            commandList->DrawInstanced(6, 1, 0, 0);
        }
    }

    if (settings.showColliderGizmos && !settings.lines.empty() && pipelineManager_->HasPipeline("DebugLines")) {
        const size_t vertexCount = settings.lines.size();
        const size_t byteSize = sizeof(DebugLineVertex) * vertexCount;
        auto *vertexBuffer = resourceContainer_->GetOrCreateVertexBuffer("EditorDebugLines", byteSize);
        if (vertexBuffer) {
            if (auto *mapped = vertexBuffer->Map()) {
                std::memcpy(mapped, settings.lines.data(), byteSize);
            }

            pipelineBinder.UsePipeline("DebugLines");
            auto &shaderBinder = pipelineManager_->GetShaderVariableBinder(Passkey<Renderer>{}, "DebugLines");
            shaderBinder.SetCommandList(commandList);
            shaderBinder.Bind("Vertex:gCamera3D", cameraBuffer);
            pipelineBinder.SetVertexBuffer(vertexBuffer, sizeof(DebugLineVertex));
            commandList->DrawInstanced(static_cast<UINT>(vertexCount), 1, 0, 0);
        }
    }
}

void Renderer::RenderEditorBackground(ScreenBuffer *screenBuffer,
    PipelineBinder &pipelineBinder,
    SceneRenderer *sceneRenderer) {
    if (!screenBuffer || !sceneRenderer) return;

    // エディター用描画先でなければ何もしない（通常の描画先の背景には影響しない）
    if (!sceneRenderer->GetEditorCameraBuffer(screenBuffer)) return;
    if (!pipelineManager_->HasPipeline("Background")) return;

    const auto &settings = sceneRenderer->GetEditorDebugDraw();
    auto *commandList = screenBuffer->GetCommandList();
    if (!commandList) return;

    struct BackgroundConstant {
        Vector4 color{ 0.0f, 0.0f, 0.0f, 1.0f };
        float useTexture = 0.0f;
        float padding[3]{};
    };
    BackgroundConstant constant{};
    constant.color = settings.backgroundColor;
    constant.useTexture = (settings.backgroundTextureHandle != TextureManager::kInvalidHandle) ? 1.0f : 0.0f;

    auto *constantBuffer = resourceContainer_->GetOrCreateConstantBuffer("EditorBackground", sizeof(BackgroundConstant));
    if (!constantBuffer) return;
    if (auto *mapped = constantBuffer->Map()) {
        std::memcpy(mapped, &constant, sizeof(constant));
    }

    pipelineBinder.UsePipeline("Background");
    auto &shaderBinder = pipelineManager_->GetShaderVariableBinder(Passkey<Renderer>{}, "Background");
    shaderBinder.SetCommandList(commandList);
    shaderBinder.Bind("Pixel:BackgroundCB", constantBuffer);

    if (constant.useTexture > 0.5f) {
        TextureManager::BindTexture(&shaderBinder, "Pixel:gTexture", settings.backgroundTextureHandle);
    } else {
        const auto fallbackHandle = TextureManager::GetTextureFromFileName("white1x1.png");
        if (fallbackHandle != TextureManager::kInvalidHandle) {
            TextureManager::BindTexture(&shaderBinder, "Pixel:gTexture", fallbackHandle);
        }
    }
    SamplerManager::BindSampler(&shaderBinder, "Pixel:gSampler", DefaultSampler::LinearClamp);

    commandList->DrawInstanced(3, 1, 0, 0);
}

void Renderer::RenderPostProcess(ScreenBuffer *screenBuffer,
    PipelineBinder &pipelineBinder,
    EmptyObject *ownerObject) {
    if (!screenBuffer || !ownerObject) return;

    auto postProcessComponents = ownerObject->GetComponents<IPostProcessComponent>();
    if (postProcessComponents.empty()) return;

    auto *commandList = screenBuffer->GetCommandList();
    if (!commandList) return;

    for (auto *component : postProcessComponents) {
        if (!component || !component->IsActive()) continue;
        // 除外設定されているスクリーンバッファには適用しない
        if (!component->IsScreenBufferIncluded(screenBuffer)) continue;

        // 中間レンダーターゲットを使う多段パス等はコンポーネント側のカスタム描画で行う
        IPostProcessComponent::CustomRenderContext customContext;
        customContext.screenBuffer = screenBuffer;
        customContext.commandList = commandList;
        customContext.pipelineManager = pipelineManager_;
        customContext.pipelineBinder = &pipelineBinder;
        customContext.getShaderBinder = [this, commandList](const std::string &name) -> ShaderVariableBinder & {
            auto &binder = pipelineManager_->GetShaderVariableBinder(Passkey<Renderer>{}, name);
            binder.SetCommandList(commandList);
            return binder;
        };
        if (component->RenderCustomInterface(Passkey<Renderer>{}, customContext)) {
            continue;
        }

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
