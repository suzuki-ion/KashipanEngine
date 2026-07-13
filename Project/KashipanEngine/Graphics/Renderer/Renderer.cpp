#include "Renderer.h"

#include <algorithm>
#include <cmath>
#include <cstring>
#include <map>
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
#include "Graphics/Resources/DepthStencilResource.h"
#include "Graphics/ScreenBuffer.h"
#include "Math/Vector3.h"
#include "Objects/Components/Compute/ComputeShaderProcessing.h"
#include "Objects/Components/PostProcessing/IPostProcessComponent.h"
#include "Objects/Components/Render/CameraRenderer.h"
#include "Objects/Components/Render/Light.h"
#include "Objects/Components/Render/LightRenderer.h"
#include "Objects/Components/Render/MeshRenderer.h"
#include "Objects/Components/Render/SkinnedMeshRenderer.h"
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
    /// @brief 影を生成するライトのスロット番号（影を生成しない場合は -1）
    std::int32_t shadowMapIndex = -1;
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
    /// @brief 影を生成するライトのスロット番号（影を生成しない場合は -1）
    std::int32_t shadowMapIndex = -1;
};

/// @brief gDirectionalLights 構造化バッファと同レイアウトの構造体
struct DirectionalLightElement {
    std::uint32_t enabled = 0;
    Vector4 color{ 1.0f, 1.0f, 1.0f, 1.0f };
    Vector3 direction{ 0.0f, -1.0f, 0.0f };
    float intensity = 1.0f;
    /// @brief 影を生成するライトのスロット番号（影を生成しない場合は -1）
    std::int32_t shadowMapIndex = -1;
};
#pragma pack(pop)

/// @brief LightCounts 定数バッファと同レイアウトの構造体
struct LightCountsData {
    std::uint32_t pointLightCount = 0;
    std::uint32_t spotLightCount = 0;
    std::uint32_t directionalLightCount = 0;
    std::uint32_t padding = 0;
};

/// @brief ShadowMapConstants 定数バッファ内の1ライト分のデータ（HLSL側の ShadowLightData と同レイアウト）
struct ShadowLightConstants {
    /// @brief Directional: 0..3=カスケード / Spot: 0のみ / Point: 0..5=+X,-X,+Y,-Y,+Z,-Z
    Matrix4x4 viewProjections[Renderer::kMaxShadowViewProjections];
    /// @brief 各カスケードの適用終端（カメラビュー空間の深度）。HLSL側は float4
    float cascadeSplits[4]{};
    /// @brief x: 1テクセルのUVサイズ / y: 配列内の先頭スライス番号 / z: ライト種別 / w: 透視投影の深度バイアス係数
    float params[4]{};
    /// @brief カスケードごとの深度バイアス係数（Directional用）。HLSL側は float4
    float cascadeBiasScales[4]{};
};

/// @brief ShadowMapConstants 定数バッファと同レイアウトの構造体
struct ShadowMapConstantsData {
    ShadowLightConstants lights[Renderer::kMaxShadowLightsPerTarget];
    std::uint32_t shadowLightCount = 0;
    float padding[3]{};
};

/// @brief シャドウパスでライトカメラとしてバインドする gCamera3D と同レイアウトの構造体
struct LightCameraConstantData {
    Matrix4x4 view = Matrix4x4::Identity();
    Matrix4x4 projection = Matrix4x4::Identity();
    Matrix4x4 viewProjection = Matrix4x4::Identity();
    Vector4 eyePosition{ 0.0f, 0.0f, 0.0f, 1.0f };
    float fov = 0.0f;
    float padding[3]{};
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

/// @brief 指定の描画先が対象オブジェクトの描画先に含まれるか（未指定の場合は全描画先に適用）
bool IsTargetMatch(EmptyObject *targetObject, bool hasTargetSpecified, IRenderTarget *target) {
    if (!hasTargetSpecified) return true;
    if (!targetObject) return false; // 指定されているが解決できない場合は適用しない
    std::vector<IRenderTarget *> targets;
    SceneRenderer::CollectRenderTargets(targetObject, targets);
    return std::find(targets.begin(), targets.end(), target) != targets.end();
}

/// @brief EditorOnlyオブジェクト（祖先を含む）のコンポーネントを、エディター用以外の描画先から除外するか
/// @details ライト・カメラがEditorOnlyオブジェクトに付いている場合、エディターのシーンビュー以外へは適用しない
bool IsExcludedAsEditorOnly(const IObjectComponent *component, const IRenderTarget *target, const SceneRenderer *sceneRenderer) {
    if (!component || !sceneRenderer) return false;
    if (target == sceneRenderer->GetEditorTarget()) return false;
    const EmptyObject *owner = component->GetOwnerObject();
    return owner && owner->IsEditorOnlyInHierarchy();
}

} // namespace

Renderer::Renderer(Passkey<GraphicsEngine>, DirectXCommon *directXCommon, PipelineManager *pipelineManager)
    : directXCommon_(directXCommon), pipelineManager_(pipelineManager) {
    resourceContainer_ = std::make_unique<ResourceContainer>();
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

    // 今フレームで描画される描画先ごとに、その描画先で使うカメラ・ライトからシャドウマップを生成する
    // （他の描画パスより先に実行する）
    {
        std::vector<IRenderTarget *> shadowTargets;
        for (const auto &entry : drawList) {
            if (std::find(shadowTargets.begin(), shadowTargets.end(), entry.target) == shadowTargets.end()) {
                shadowTargets.push_back(entry.target);
            }
        }
        RenderShadowMaps(sceneContext, sceneRenderer, shadowTargets);
    }

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

void Renderer::RenderShadowMaps(SceneContext *sceneContext, SceneRenderer *sceneRenderer,
    const std::vector<IRenderTarget *> &targets) {
    (void)sceneContext;
    shadowJobs_.clear();
    targetShadowEntries_.clear();
    if (!sceneRenderer || !directXCommon_ || !pipelineManager_) return;

    constexpr const char *kShadowPipelineName = "Object3D.ShadowMap.DepthOnly";
    /// 1フレームで使えるシャドウマップ配列のスライス数の予算
    constexpr std::uint32_t kMaxShadowSlices = 64;
    /// シャドウマップ配列のメモリ予算（超える場合は解像度を自動で下げる）
    constexpr std::uint64_t kShadowMemoryBudgetBytes = 512ull * 1024 * 1024;

    // コマンドスロットの確保（初回のみ）
    auto ensureShadowCommands = [this]() -> DX12Commands * {
        if (shadowCommandSlotIndex_ < 0) {
            shadowCommandSlotIndex_ = directXCommon_->AcquireCommandObjects(Passkey<Renderer>{});
            shadowCommands_ = (shadowCommandSlotIndex_ >= 0)
                ? directXCommon_->GetCommandObjects(Passkey<Renderer>{}, shadowCommandSlotIndex_)
                : nullptr;
        }
        return shadowCommands_;
    };

    // 影ジョブが1件も無いフレームでも、シェーダーの gShadowMaps（Texture2DArray）へバインドできる
    // 最小のダミー配列を用意してSRV状態へ遷移させておく（未バインドのデスクリプタアクセス防止）
    auto ensureFallbackArray = [this, &ensureShadowCommands]() {
        if (shadowMapArray_) return;
        // SRVがTexture2DArrayとして作られるようにスライス数は2以上にする
        shadowMapArray_ = std::make_unique<DepthStencilResource>(
            64, 64, DXGI_FORMAT_D32_FLOAT, 1.0f, static_cast<UINT8>(0),
            nullptr, true, DXGI_FORMAT_R32_FLOAT, 2);
        if (!shadowMapArray_ || !shadowMapArray_->HasSrv()) {
            shadowMapArray_.reset();
            return;
        }
        shadowArrayResolution_ = 64;
        shadowArraySliceCount_ = 2;
        shadowArrayReady_ = false;
        auto *commands = ensureShadowCommands();
        auto *commandList = commands ? commands->BeginRecord() : nullptr;
        if (!commandList) return;
        shadowMapArray_->SetCommandList(commandList);
        shadowMapArray_->TransitionToShaderResource();
        if (commands->EndRecord()) {
            directXCommon_->AddRecordCommandList(Passkey<Renderer>{}, commands->GetCommandList());
            shadowArrayReady_ = true;
        }
    };

    //--------- 解像度の決定（影を生成する全ライトの最大値。メモリ予算超過時は自動で下げる） ---------//
    std::uint32_t resolution = 0;
    std::uint64_t estimatedSlices = 0;
    if (pipelineManager_->HasPipeline(kShadowPipelineName)) {
        for (auto *lightRenderer : sceneRenderer->GetLightRenderers()) {
            if (!lightRenderer || !lightRenderer->IsActive()) continue;
            auto *light = lightRenderer->GetLight();
            if (!light || !light->IsActive() || !light->IsCastShadows()) continue;
            resolution = std::max(resolution, light->GetShadowMapResolution());
            switch (light->GetType()) {
                case Light::Type::Directional: estimatedSlices += kShadowCascadeCount; break;
                case Light::Type::Point: estimatedSlices += 6; break;
                default: estimatedSlices += 1; break;
            }
        }
    }
    if (resolution == 0 || estimatedSlices == 0) {
        ensureFallbackArray();
        return;
    }
    resolution = std::clamp(resolution, 256u, 4096u);
    const std::uint64_t budgetSlices = std::min<std::uint64_t>(estimatedSlices, kMaxShadowSlices);
    while (resolution > 256 &&
           static_cast<std::uint64_t>(resolution) * resolution * 4ull * budgetSlices > kShadowMemoryBudgetBytes) {
        resolution /= 2;
    }

    // 行ベクトル規約でNDC座標をワールド座標へ逆射影する
    const auto unprojectNdc = [](const Matrix4x4 &invViewProjection, float x, float y, float z) {
        const auto &m = invViewProjection.m;
        const float outX = x * m[0][0] + y * m[1][0] + z * m[2][0] + m[3][0];
        const float outY = x * m[0][1] + y * m[1][1] + z * m[2][1] + m[3][1];
        const float outZ = x * m[0][2] + y * m[1][2] + z * m[2][2] + m[3][2];
        const float outW = x * m[0][3] + y * m[1][3] + z * m[2][3] + m[3][3];
        const float invW = (std::fabs(outW) > 1e-6f) ? (1.0f / outW) : 1.0f;
        return Vector3(outX * invW, outY * invW, outZ * invW);
    };
    // ライトの向きからビュー行列（左手系の正規直交基底）を作る
    const auto makeLightView = [](const Vector3 &forward, const Vector3 &eye) {
        const Vector3 baseUp = (std::fabs(forward.y) > 0.99f) ? Vector3(0.0f, 0.0f, 1.0f) : Vector3(0.0f, 1.0f, 0.0f);
        const Vector3 right = baseUp.Cross(forward).Normalize();
        const Vector3 up = forward.Cross(right);
        const Matrix4x4 world(
            right.x, right.y, right.z, 0.0f,
            up.x, up.y, up.z, 0.0f,
            forward.x, forward.y, forward.z, 0.0f,
            eye.x, eye.y, eye.z, 1.0f);
        return world.Inverse();
    };

    //--------- 描画先ごとに「その描画先で使うカメラ・ライト」から影ジョブを構築する ---------//
    // ジョブの共有キー: Directionalはカメラ依存のため（ライト, カメラ）、Spot/Pointは（ライト, null）
    std::map<std::pair<const LightRenderer *, const void *>, int> jobIndexByKey;
    std::uint32_t nextSlice = 0;

    for (auto *target : targets) {
        if (!target) continue;

        // 描画先で使うカメラの解決（エディター用描画先はエディターカメラを使用する）
        Matrix4x4 cameraViewProjection = Matrix4x4::Identity();
        Vector3 cameraPosition(0.0f, 0.0f, 0.0f);
        float cameraNear = 0.1f;
        float cameraFar = 1000.0f;
        bool cameraValid = false;
        const void *cameraKey = nullptr;
        if (const auto *editorInfo = sceneRenderer->GetEditorCameraInfo(target)) {
            cameraViewProjection = editorInfo->viewProjection;
            cameraPosition = editorInfo->position;
            cameraNear = editorInfo->nearClip;
            cameraFar = editorInfo->farClip;
            cameraValid = true;
            cameraKey = target;
        } else {
            for (auto *cameraRenderer : sceneRenderer->GetCameraRenderers()) {
                if (!cameraRenderer || !cameraRenderer->IsActive()) continue;
                if (IsExcludedAsEditorOnly(cameraRenderer, target, sceneRenderer)) continue;
                if (!IsTargetMatch(cameraRenderer->GetTargetObject(), cameraRenderer->GetTargetObjectID().IsValid(), target)) continue;
                if (!cameraRenderer->IsRenderTargetIncluded(target)) continue;
                cameraViewProjection = cameraRenderer->GetViewProjectionMatrix();
                cameraPosition = cameraRenderer->GetWorldPosition();
                cameraNear = cameraRenderer->GetNearClip();
                cameraFar = cameraRenderer->GetFarClip();
                cameraValid = true;
                cameraKey = cameraRenderer;
                break;
            }
        }
        if (!cameraValid) continue; // カメラの無い描画先（シャドウマップ等）には影を適用しない
        cameraNear = std::max(0.01f, cameraNear);
        cameraFar = std::max(cameraNear + 0.01f, cameraFar);

        // この描画先に適用される「影を生成するライト」を収集する
        std::vector<LightRenderer *> candidates;
        for (auto *lightRenderer : sceneRenderer->GetLightRenderers()) {
            if (!lightRenderer || !lightRenderer->IsActive()) continue;
            auto *light = lightRenderer->GetLight();
            if (!light || !light->IsActive() || !light->IsCastShadows()) continue;
            if (IsExcludedAsEditorOnly(lightRenderer, target, sceneRenderer)) continue;
            if (!IsTargetMatch(lightRenderer->GetTargetObject(), lightRenderer->GetTargetObjectID().IsValid(), target)) continue;
            if (!lightRenderer->IsRenderTargetIncluded(target)) continue;
            candidates.push_back(lightRenderer);
        }
        if (candidates.empty()) continue;

        // ライトが多すぎる場合はカメラに近い順に優先する
        // （Directionalは画面全体に影響するため距離に関わらず最優先とする）
        std::stable_sort(candidates.begin(), candidates.end(),
            [&cameraPosition](LightRenderer *a, LightRenderer *b) {
                const bool aDirectional = a->GetLight()->GetType() == Light::Type::Directional;
                const bool bDirectional = b->GetLight()->GetType() == Light::Type::Directional;
                if (aDirectional != bDirectional) return aDirectional;
                const Vector3 toA = a->GetWorldPosition() - cameraPosition;
                const Vector3 toB = b->GetWorldPosition() - cameraPosition;
                return toA.Dot(toA) < toB.Dot(toB);
            });

        // 視錐台コーナー（Directionalのカスケード計算用。必要になった時点で一度だけ計算する）
        bool cornersComputed = false;
        Vector3 nearCorners[4]{};
        Vector3 farCorners[4]{};
        constexpr float kCornerX[4] = { -1.0f, 1.0f, 1.0f, -1.0f };
        constexpr float kCornerY[4] = { 1.0f, 1.0f, -1.0f, -1.0f };

        auto &entries = targetShadowEntries_[target];
        for (auto *lightRenderer : candidates) {
            if (entries.size() >= kMaxShadowLightsPerTarget) break;
            auto *light = lightRenderer->GetLight();
            const Light::Type type = light->GetType();

            const void *jobCameraKey = (type == Light::Type::Directional) ? cameraKey : nullptr;
            const auto mapKey = std::make_pair(static_cast<const LightRenderer *>(lightRenderer), jobCameraKey);
            int jobIndex = -1;
            if (auto found = jobIndexByKey.find(mapKey); found != jobIndexByKey.end()) {
                jobIndex = found->second;
            } else {
                const std::uint32_t neededSlices =
                    (type == Light::Type::Directional) ? kShadowCascadeCount :
                    (type == Light::Type::Point) ? 6u : 1u;
                if (nextSlice + neededSlices > kMaxShadowSlices) continue; // スライス予算切れ（優先度の低いライトから影を諦める）

                ShadowJobData job{};
                job.baseSlice = nextSlice;
                job.sliceCount = neededSlices;

                if (type == Light::Type::Directional) {
                    //--------- Directional: カメラ視錐台をカスケード分割して正射影をフィットさせる ---------//
                    job.lightType = 0;
                    if (!cornersComputed) {
                        const Matrix4x4 invViewProjection = cameraViewProjection.Inverse();
                        for (int i = 0; i < 4; ++i) {
                            nearCorners[i] = unprojectNdc(invViewProjection, kCornerX[i], kCornerY[i], 0.0f);
                            farCorners[i] = unprojectNdc(invViewProjection, kCornerX[i], kCornerY[i], 1.0f);
                        }
                        cornersComputed = true;
                    }

                    // カスケード分割距離（対数分割と均等分割のブレンド）
                    const float maxDistance = std::clamp(light->GetShadowDistance(), cameraNear + 0.01f, cameraFar);
                    float splitDistances[kShadowCascadeCount + 1];
                    splitDistances[0] = cameraNear;
                    constexpr float kSplitLambda = 0.75f;
                    for (std::uint32_t i = 1; i <= kShadowCascadeCount; ++i) {
                        const float p = static_cast<float>(i) / static_cast<float>(kShadowCascadeCount);
                        const float logDistance = cameraNear * std::pow(maxDistance / cameraNear, p);
                        const float uniformDistance = cameraNear + (maxDistance - cameraNear) * p;
                        splitDistances[i] = kSplitLambda * logDistance + (1.0f - kSplitLambda) * uniformDistance;
                    }

                    const Vector3 lightDirection = lightRenderer->GetWorldDirection();
                    for (std::uint32_t c = 0; c < kShadowCascadeCount; ++c) {
                        // このカスケードが覆う視錐台スライスの8頂点（視錐台の辺に沿った線形補間で求まる）
                        const float tNear = (splitDistances[c] - cameraNear) / (cameraFar - cameraNear);
                        const float tFar = (splitDistances[c + 1] - cameraNear) / (cameraFar - cameraNear);
                        Vector3 corners[8];
                        for (int i = 0; i < 4; ++i) {
                            corners[i] = nearCorners[i] + (farCorners[i] - nearCorners[i]) * tNear;
                            corners[i + 4] = nearCorners[i] + (farCorners[i] - nearCorners[i]) * tFar;
                        }

                        // スライスを内包する球で正射影範囲をフィットさせる（カメラの回転に対して影が安定する）
                        Vector3 center(0.0f, 0.0f, 0.0f);
                        for (const auto &corner : corners) center = center + corner;
                        center = center * (1.0f / 8.0f);
                        float radius = 0.0f;
                        for (const auto &corner : corners) radius = std::max(radius, (corner - center).Length());
                        radius = std::ceil(radius * 16.0f) / 16.0f;
                        radius = std::max(radius, 0.5f);

                        // ライト後方へ引いた位置から深度範囲を確保する（スライス外のキャスターも影を落とせるように）
                        const float backDistance = radius * 2.0f + 10.0f;
                        const Vector3 eye = center - lightDirection * backDistance;
                        const Matrix4x4 lightView = makeLightView(lightDirection, eye);
                        Matrix4x4 lightProjection;
                        lightProjection.MakeOrthographicMatrix(-radius, radius, radius, -radius, 0.0f, backDistance + radius);
                        Matrix4x4 viewProjection = lightView * lightProjection;

                        // 投影原点をテクセル単位にスナップしてカメラ移動時の影のちらつきを抑える
                        const float halfResolution = static_cast<float>(resolution) * 0.5f;
                        const float offsetX = viewProjection.m[3][0] * halfResolution;
                        const float offsetY = viewProjection.m[3][1] * halfResolution;
                        viewProjection.m[3][0] += (std::round(offsetX) - offsetX) / halfResolution;
                        viewProjection.m[3][1] += (std::round(offsetY) - offsetY) / halfResolution;

                        job.viewProjections[c] = viewProjection;
                        job.cascadeSplits[c] = splitDistances[c + 1];
                        // 深度バイアス係数: 1テクセルのワールドサイズを正射影の深度レンジで正規化した値。
                        // シェーダー側で「テクセル数×この値」をNDC深度バイアスとして使うことで、
                        // カスケードの大きさに関わらずワールド空間で一定（テクセル比例）のバイアスになり、
                        // 影が浮くピーターパン現象を防ぐ
                        const float texelWorldSize = (radius * 2.0f) / static_cast<float>(resolution);
                        job.cascadeBiasScales[c] = texelWorldSize / (backDistance + radius);
                    }
                } else if (type == Light::Type::Spot) {
                    //--------- Spot: ライト位置からコーン方向への透視投影1面 ---------//
                    job.lightType = 1;
                    const Vector3 lightDirection = lightRenderer->GetWorldDirection();
                    const Vector3 lightPosition = lightRenderer->GetWorldPosition();
                    // 外側コーン角の2倍を画角にする（コーン全体を覆う）
                    const float fovY = std::clamp(light->GetOuterAngle() * 2.0f, 0.05f, 3.1f);
                    const float nearZ = 0.05f;
                    const float farZ = std::max(nearZ + 0.1f, light->GetDistance());
                    Matrix4x4 lightProjection;
                    lightProjection.MakePerspectiveFovMatrix(fovY, 1.0f, nearZ, farZ);
                    job.viewProjections[0] = makeLightView(lightDirection, lightPosition) * lightProjection;
                    // 深度バイアス係数: シェーダー側でビュー深度wで割ることで、その距離での
                    // 1テクセルのワールドサイズに比例したNDC深度バイアスになる
                    // （透視投影はNDC深度が非線形のため、定数NDCバイアスだと遠方で影が大きく浮いてしまう）
                    job.perspectiveBiasScale = (2.0f * std::tan(fovY * 0.5f) / static_cast<float>(resolution))
                        * (nearZ * farZ / (farZ - nearZ));
                } else {
                    //--------- Point: ライト位置からキューブ6面（画角90度）の透視投影 ---------//
                    job.lightType = 2;
                    const Vector3 lightPosition = lightRenderer->GetWorldPosition();
                    const float nearZ = 0.05f;
                    const float farZ = std::max(nearZ + 0.1f, light->GetRadius());
                    Matrix4x4 lightProjection;
                    lightProjection.MakePerspectiveFovMatrix(1.5707963f, 1.0f, nearZ, farZ);
                    // 深度バイアス係数（Spotと同様。画角90度なので tan(fov/2) = 1）
                    job.perspectiveBiasScale = (2.0f / static_cast<float>(resolution))
                        * (nearZ * farZ / (farZ - nearZ));
                    // 面の並び順はシェーダー側の面選択（+X,-X,+Y,-Y,+Z,-Z）と一致させること
                    const Vector3 kFaceDirections[6] = {
                        Vector3(1.0f, 0.0f, 0.0f), Vector3(-1.0f, 0.0f, 0.0f),
                        Vector3(0.0f, 1.0f, 0.0f), Vector3(0.0f, -1.0f, 0.0f),
                        Vector3(0.0f, 0.0f, 1.0f), Vector3(0.0f, 0.0f, -1.0f),
                    };
                    for (int face = 0; face < 6; ++face) {
                        job.viewProjections[face] = makeLightView(kFaceDirections[face], lightPosition) * lightProjection;
                    }
                }

                shadowJobs_.push_back(job);
                jobIndex = static_cast<int>(shadowJobs_.size()) - 1;
                jobIndexByKey.emplace(mapKey, jobIndex);
                nextSlice += neededSlices;
            }
            entries.push_back({ lightRenderer, jobIndex });
        }
        if (entries.empty()) targetShadowEntries_.erase(target);
    }

    if (shadowJobs_.empty()) {
        targetShadowEntries_.clear();
        ensureFallbackArray();
        return;
    }

    //--------- シャドウマップ配列（Texture2DArray）の生成・拡張 ---------//
    // SRVがTexture2DArrayとして作られるようにスライス数は2以上にする
    const std::uint32_t requiredSlices = std::max(2u, nextSlice);
    if (!shadowMapArray_ || shadowArrayResolution_ != resolution || shadowArraySliceCount_ < requiredSlices) {
        // 頻繁な作り直しを避けるため、同解像度の場合はスライス数を拡大方向にのみ変更する
        const std::uint32_t newSliceCount = (shadowMapArray_ && shadowArrayResolution_ == resolution)
            ? std::max(requiredSlices, shadowArraySliceCount_)
            : requiredSlices;
        shadowMapArray_ = std::make_unique<DepthStencilResource>(
            resolution, resolution, DXGI_FORMAT_D32_FLOAT, 1.0f, static_cast<UINT8>(0),
            nullptr, true, DXGI_FORMAT_R32_FLOAT, newSliceCount);
        if (!shadowMapArray_ || !shadowMapArray_->HasSrv()) {
            shadowMapArray_.reset();
            shadowArrayResolution_ = 0;
            shadowArraySliceCount_ = 0;
            shadowArrayReady_ = false;
            shadowJobs_.clear();
            targetShadowEntries_.clear();
            return;
        }
        shadowArrayResolution_ = resolution;
        shadowArraySliceCount_ = newSliceCount;
        shadowArrayReady_ = false;
    }

    //--------- 影を落とす3Dオブジェクトの収集（MeshRenderer / SkinnedMeshRenderer） ---------//
    struct ShadowDrawSource {
        ModelManager::ModelHandle meshHandle = ModelManager::kInvalidHandle;
        MaterialManager::MaterialHandle materialHandle = MaterialManager::kInvalidHandle;
        Matrix4x4 worldMatrix;
        RWStructuredBufferResource *skinnedVertexBuffer = nullptr;
    };
    std::vector<ShadowDrawSource> sources;
    const auto isShadowCastingPipeline = [](const std::string &name) {
        // 3Dオブジェクト描画用パイプラインのみ対象（シャドウマップ用パイプライン自体は除外）
        return name.rfind("Object3D", 0) == 0 && name.rfind("Object3D.ShadowMap", 0) != 0;
    };
    for (auto *renderer : sceneRenderer->GetMeshRenderers()) {
        if (!renderer || !renderer->IsActive()) continue;
        if (renderer->GetMeshHandle() == ModelManager::kInvalidHandle) continue;
        if (!isShadowCastingPipeline(renderer->GetPipelineName())) continue;
        sources.push_back({ renderer->GetMeshHandle(), renderer->GetMaterialHandle(), renderer->GetWorldMatrix(), nullptr });
    }
    for (auto *renderer : sceneRenderer->GetSkinnedMeshRenderers()) {
        if (!renderer || !renderer->IsActive()) continue;
        if (renderer->GetMeshHandle() == ModelManager::kInvalidHandle) continue;
        if (!renderer->HasValidSkinningData()) continue;
        if (!isShadowCastingPipeline(renderer->GetPipelineName())) continue;
        sources.push_back({ renderer->GetMeshHandle(), renderer->GetMaterialHandle(), renderer->GetWorldMatrix(), renderer->GetSkinnedVertexBuffer() });
    }

    // 同一（メッシュ・マテリアル）をまとめてインスタンシング描画できるようにソート
    std::stable_sort(sources.begin(), sources.end(),
        [](const ShadowDrawSource &a, const ShadowDrawSource &b) {
            if (a.skinnedVertexBuffer != b.skinnedVertexBuffer) return a.skinnedVertexBuffer < b.skinnedVertexBuffer;
            if (a.meshHandle != b.meshHandle) return a.meshHandle < b.meshHandle;
            return a.materialHandle < b.materialHandle;
        });

    //--------- コマンド記録開始 ---------//
    auto *commands = ensureShadowCommands();
    auto *commandList = commands ? commands->BeginRecord() : nullptr;
    if (!commandList) {
        shadowJobs_.clear();
        targetShadowEntries_.clear();
        return;
    }

    // このコマンドリストはフレームごとにResetされるため、ヒープとパイプラインの状態を毎回設定する
    auto *srvHeap = IGraphicsResource::GetSRVHeap(Passkey<Renderer>{});
    auto *samplerHeap = IGraphicsResource::GetSamplerHeap(Passkey<Renderer>{});
    if (srvHeap && samplerHeap) {
        ID3D12DescriptorHeap *heaps[] = { srvHeap->GetDescriptorHeap(), samplerHeap->GetDescriptorHeap() };
        commandList->SetDescriptorHeaps(_countof(heaps), heaps);
    }
    PipelineBinder pipelineBinder(commandList, pipelineManager_);
    pipelineBinder.Invalidate();
    pipelineBinder.UsePipeline(kShadowPipelineName);
    auto &shaderBinder = pipelineManager_->GetShaderVariableBinder(Passkey<Renderer>{}, kShadowPipelineName);
    shaderBinder.SetCommandList(commandList);

    //--------- バッチごとのGPUバッファ準備（全ジョブ・全スライスで共有する） ---------//
    struct PreparedShadowBatch {
        const ResourceContainer::MeshBuffers *meshBuffers = nullptr;
        RWStructuredBufferResource *skinnedVertexBuffer = nullptr;
        StructuredBufferResource *transformBuffer = nullptr;
        StructuredBufferResource *materialBuffer = nullptr;
        std::uint32_t textureHandle = TextureManager::kInvalidHandle;
        SamplerManager::SamplerHandle samplerHandle = SamplerManager::kInvalidHandle;
        std::uint32_t instanceCount = 0;
    };
    std::vector<PreparedShadowBatch> batches;
    const auto fallbackTextureHandle = TextureManager::GetTextureFromFileName("white1x1.png");
    {
        size_t begin = 0;
        std::uint32_t batchIndex = 0;
        while (begin < sources.size()) {
            const auto &first = sources[begin];
            size_t end = begin;
            while (end < sources.size() &&
                   sources[end].meshHandle == first.meshHandle &&
                   sources[end].materialHandle == first.materialHandle &&
                   sources[end].skinnedVertexBuffer == first.skinnedVertexBuffer) {
                ++end;
            }
            const std::uint32_t instanceCount = static_cast<std::uint32_t>(end - begin);

            const auto *meshBuffers = resourceContainer_->GetOrCreateMeshBuffers(first.meshHandle);
            if (!meshBuffers || !meshBuffers->indexBuffer ||
                (!first.skinnedVertexBuffer && !meshBuffers->vertexBuffer)) {
                begin = end;
                continue;
            }

            PreparedShadowBatch batch;
            batch.meshBuffers = meshBuffers;
            batch.skinnedVertexBuffer = first.skinnedVertexBuffer;
            batch.instanceCount = instanceCount;

            // ワールド行列のインスタンスバッファ（カスケード間で内容は共通）
            char key[64];
            std::snprintf(key, sizeof(key), "ShadowPass|%u|transform", batchIndex);
            batch.transformBuffer = resourceContainer_->GetOrCreateStructuredBuffer(key, sizeof(Matrix4x4), instanceCount);
            if (batch.transformBuffer) {
                if (auto *mapped = static_cast<Matrix4x4 *>(batch.transformBuffer->Map())) {
                    for (size_t i = begin; i < end; ++i) {
                        mapped[i - begin] = sources[i].worldMatrix;
                    }
                }
            }

            // マテリアルの構造化バッファ（シャドウマップ用PSがアルファ抜きに使用する）
            auto *material = MaterialManager::GetMaterial(first.materialHandle);
            if (material) {
                material->ResolveTextureHandles();
            }
            MaterialElement element;
            if (material) {
                element.enableLighting = material->enableLighting ? 1.0f : 0.0f;
                element.enableEnvironmentMapping = 0.0f;
                element.enableShadowMapProjection = material->enableShadowMapProjection ? 1.0f : 0.0f;
                element.useTexture = (material->textureHandle != TextureManager::kInvalidHandle) ? 1.0f : 0.0f;
                element.color = material->color;
                element.uvTransform = material->uvTransform;
                element.shininess = material->shininess;
                element.specularColor = material->specularColor;
                element.environmentCoefficient = material->environmentCoefficient;
                batch.textureHandle = material->textureHandle;
                batch.samplerHandle = material->samplerHandle;
            }
            std::snprintf(key, sizeof(key), "ShadowPass|%u|material", batchIndex);
            batch.materialBuffer = resourceContainer_->GetOrCreateStructuredBuffer(key, sizeof(MaterialElement), instanceCount);
            if (batch.materialBuffer) {
                if (auto *mapped = static_cast<MaterialElement *>(batch.materialBuffer->Map())) {
                    for (std::uint32_t i = 0; i < instanceCount; ++i) {
                        mapped[i] = element;
                    }
                }
            }

            if (batch.transformBuffer && batch.materialBuffer) {
                batches.push_back(batch);
                ++batchIndex;
            }
            begin = end;
        }
    }

    //--------- シャドウマップ配列を深度書き込み状態にして全スライスを一括クリア ---------//
    shadowMapArray_->SetCommandList(commandList);
    shadowMapArray_->TransitionTo(D3D12_RESOURCE_STATE_DEPTH_WRITE);
    {
        const auto fullDsv = shadowMapArray_->GetCPUDescriptorHandle();
        commandList->OMSetRenderTargets(0, nullptr, FALSE, &fullDsv);
        shadowMapArray_->ClearDepthStencilView();
    }
    {
        const float resolutionF = static_cast<float>(resolution);
        D3D12_VIEWPORT viewport{ 0.0f, 0.0f, resolutionF, resolutionF, 0.0f, 1.0f };
        D3D12_RECT scissor{ 0, 0, static_cast<LONG>(resolution), static_cast<LONG>(resolution) };
        commandList->RSSetViewports(1, &viewport);
        commandList->RSSetScissorRects(1, &scissor);
    }

    //--------- 影ジョブ × スライス数だけシャドウマップ描画パスを回す ---------//
    for (size_t jobIndex = 0; jobIndex < shadowJobs_.size(); ++jobIndex) {
        const auto &job = shadowJobs_[jobIndex];
        for (std::uint32_t s = 0; s < job.sliceCount; ++s) {
            const auto dsv = shadowMapArray_->GetSliceDsvHandle(job.baseSlice + s);
            commandList->OMSetRenderTargets(0, nullptr, FALSE, &dsv);

            // ライトカメラの定数バッファ（ShadowMapVS が gCamera3D.viewProjection を参照する）
            char cameraKey[64];
            std::snprintf(cameraKey, sizeof(cameraKey), "ShadowPass|%zu|%u|camera", jobIndex, s);
            auto *cameraBuffer = resourceContainer_->GetOrCreateConstantBuffer(cameraKey, sizeof(LightCameraConstantData));
            if (!cameraBuffer) continue;
            if (auto *mapped = cameraBuffer->Map()) {
                LightCameraConstantData constant;
                constant.viewProjection = job.viewProjections[s];
                std::memcpy(mapped, &constant, sizeof(constant));
            }

            for (const auto &batch : batches) {
                shaderBinder.Bind("Vertex:gCamera3D", cameraBuffer);
                shaderBinder.Bind("Vertex:gTransformationMatrices", batch.transformBuffer);
                shaderBinder.Bind("Pixel:gMaterials", batch.materialBuffer);
                if (batch.textureHandle != TextureManager::kInvalidHandle) {
                    TextureManager::BindTexture(&shaderBinder, "Pixel:gTexture", batch.textureHandle);
                } else if (fallbackTextureHandle != TextureManager::kInvalidHandle) {
                    TextureManager::BindTexture(&shaderBinder, "Pixel:gTexture", fallbackTextureHandle);
                }
                if (batch.samplerHandle != SamplerManager::kInvalidHandle) {
                    SamplerManager::BindSampler(&shaderBinder, "Pixel:gSampler", batch.samplerHandle);
                } else {
                    SamplerManager::BindSampler(&shaderBinder, "Pixel:gSampler", DefaultSampler::LinearWrap);
                }

                if (batch.skinnedVertexBuffer) {
                    batch.skinnedVertexBuffer->SetCommandList(commandList);
                    D3D12_VERTEX_BUFFER_VIEW skinnedView = batch.skinnedVertexBuffer->GetView(sizeof(ResourceContainer::MeshVertex));
                    pipelineBinder.SetVertexBufferView(0, 1, &skinnedView);
                } else {
                    pipelineBinder.SetVertexBuffer(batch.meshBuffers->vertexBuffer.get(), sizeof(ResourceContainer::MeshVertex));
                }
                pipelineBinder.SetIndexBuffer(batch.meshBuffers->indexBuffer.get());
                commandList->DrawIndexedInstanced(batch.meshBuffers->indexCount, batch.instanceCount, 0, 0, 0);
            }
        }
    }

    //--------- 配列全体をシェーダーから参照可能な状態へ遷移して記録終了 ---------//
    shadowMapArray_->TransitionToShaderResource();
    if (commands->EndRecord()) {
        directXCommon_->AddRecordCommandList(Passkey<Renderer>{}, commands->GetCommandList());
        shadowArrayReady_ = true;
    } else {
        shadowJobs_.clear();
        targetShadowEntries_.clear();
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
        // EditorOnlyオブジェクトのカメラはエディター用以外の描画先にはバインドしない
        if (IsExcludedAsEditorOnly(cameraRenderer, target, sceneRenderer)) continue;
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
    for (auto *lightRenderer : sceneRenderer->GetLightRenderers()) {
        if (!lightRenderer || !lightRenderer->IsActive()) continue;
        // EditorOnlyオブジェクトのライトはエディター用以外の描画先には適用しない
        if (IsExcludedAsEditorOnly(lightRenderer, target, sceneRenderer)) continue;
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
            // このライトが影を生成する場合はシャドウマップのスロット番号を対応付ける
            element.shadowMapIndex = findShadowIndex(lightRenderer);
            directionalLights.push_back(element);
        } else if (light->GetType() == Light::Type::Point) {
            PointLightElement element;
            element.enabled = light->IsActive() ? 1u : 0u;
            element.color = light->GetColor();
            element.position = lightRenderer->GetWorldPosition();
            element.radius = light->GetRadius();
            element.intensity = light->GetIntensity();
            element.decay = light->GetDecay();
            element.shadowMapIndex = findShadowIndex(lightRenderer);
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
            element.shadowMapIndex = findShadowIndex(lightRenderer);
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
