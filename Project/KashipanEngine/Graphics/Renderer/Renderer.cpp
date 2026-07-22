#include "Renderer.h"

#include <algorithm>
#include <array>
#include <cmath>
#include <cstring>
#include <functional>
#include <map>
#include <utility>
#include <vector>

#include "Assets/FontManager.h"
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
#include "Math/Vector2.h"
#include "Math/Vector3.h"
#include "Objects/Components/Compute/ComputeShaderProcessing.h"
#include "Objects/Components/ParticleSystemBase.h"
#include "Objects/Components/PostProcessing/IPostProcessComponent.h"
#include "Objects/Components/Render/CameraRenderer.h"
#include "Objects/Components/Render/Light.h"
#include "Objects/Components/Render/LightRenderer.h"
#include "Objects/Components/Render/MeshRenderer.h"
#include "Objects/Components/Render/SkinnedMeshRenderer.h"
#include "Objects/Components/Render/TextRenderer.h"
#include "Assets/ModelManager.h"
#include "Graphics/Resources/RWStructuredBufferResource.h"
#include "Graphics/Resources/StructuredBufferResource.h"
#include "Objects/EmptyObject.h"
#include "Objects/Components/Transform.h"
#include "Scene/Components/Compute/SceneComputeProcessor.h"
#include "Scene/SceneContext.h"
#include "Utilities/TimeUtils.h"

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
    Vector4 rimColor{ 1.0f, 1.0f, 1.0f, 1.0f };
    float rimPower = 2.0f;
    float rimIntensity = 0.0f;
    float useNormalMap = 0.0f;
};

/// @brief gMaterials 構造化バッファ（Object2D）と同レイアウトの構造体
struct Material2DElement {
    Vector4 color{ 1.0f, 1.0f, 1.0f, 1.0f };
    Matrix4x4 uvTransform = Matrix4x4::Identity();
    float useTexture = 1.0f;
    float padding[3]{};
};

/// @brief gMaterials 構造化バッファ（Text2D、TextSDFPS.hlslのTextCharacterElement）と同レイアウトの構造体
struct TextCharacterElement {
    Vector4 color{ 1.0f, 1.0f, 1.0f, 1.0f };
    Vector4 uvRect{ 0.0f, 0.0f, 0.0f, 0.0f };
    float boldWeight = 0.0f;
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

/// @brief gSphereLights 構造化バッファと同レイアウトの構造体
struct SphereLightElement {
    std::uint32_t enabled = 0;
    Vector4 color{ 1.0f, 1.0f, 1.0f, 1.0f };
    Vector3 position{ 0.0f, 0.0f, 0.0f };
    float radius = 10.0f;
    float sourceRadius = 0.5f;
    float intensity = 1.0f;
    float decay = 2.0f;
    std::int32_t shadowMapIndex = -1;
};

/// @brief gDiscLights 構造化バッファと同レイアウトの構造体
struct DiscLightElement {
    std::uint32_t enabled = 0;
    Vector4 color{ 1.0f, 1.0f, 1.0f, 1.0f };
    Vector3 position{ 0.0f, 0.0f, 0.0f };
    float distance = 10.0f;
    Vector3 direction{ 0.0f, -1.0f, 0.0f };
    float sourceRadius = 0.5f;
    float intensity = 1.0f;
    float decay = 2.0f;
    std::int32_t shadowMapIndex = -1;
};

/// @brief gRectLights 構造化バッファと同レイアウトの構造体
struct RectLightElement {
    std::uint32_t enabled = 0;
    Vector4 color{ 1.0f, 1.0f, 1.0f, 1.0f };
    Vector3 position{ 0.0f, 0.0f, 0.0f };
    float distance = 10.0f;
    Vector3 direction{ 0.0f, -1.0f, 0.0f };
    Vector3 right{ 1.0f, 0.0f, 0.0f };
    float width = 1.0f;
    Vector3 up{ 0.0f, 1.0f, 0.0f };
    float height = 1.0f;
    float intensity = 1.0f;
    float decay = 2.0f;
    std::int32_t shadowMapIndex = -1;
};

/// @brief gTubeLights 構造化バッファと同レイアウトの構造体
struct TubeLightElement {
    std::uint32_t enabled = 0;
    Vector4 color{ 1.0f, 1.0f, 1.0f, 1.0f };
    Vector3 p0{ 0.0f, 0.0f, 0.0f };
    float radius = 10.0f;
    Vector3 p1{ 0.0f, 0.0f, 0.0f };
    float sourceRadius = 0.5f;
    float intensity = 1.0f;
    float decay = 2.0f;
    std::int32_t shadowMapIndex = -1;
};

/// @brief gBoxLights 構造化バッファと同レイアウトの構造体
struct BoxLightElement {
    std::uint32_t enabled = 0;
    Vector4 color{ 1.0f, 1.0f, 1.0f, 1.0f };
    Vector3 position{ 0.0f, 0.0f, 0.0f };
    float radius = 10.0f;
    Vector3 right{ 1.0f, 0.0f, 0.0f };
    float halfWidth = 0.5f;
    Vector3 up{ 0.0f, 1.0f, 0.0f };
    float halfHeight = 0.5f;
    Vector3 forward{ 0.0f, 0.0f, 1.0f };
    float halfDepth = 0.5f;
    float intensity = 1.0f;
    float decay = 2.0f;
    std::int32_t shadowMapIndex = -1;
};
#pragma pack(pop)

/// @brief LightCounts 定数バッファと同レイアウトの構造体
struct LightCountsData {
    std::uint32_t pointLightCount = 0;
    std::uint32_t spotLightCount = 0;
    std::uint32_t directionalLightCount = 0;
    std::uint32_t sphereLightCount = 0;
    std::uint32_t discLightCount = 0;
    std::uint32_t rectLightCount = 0;
    std::uint32_t tubeLightCount = 0;
    std::uint32_t boxLightCount = 0;
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
    /// @brief x: 光源サイズ（ワールド単位、PCSSの半影ソフト化に使用。0=硬い影）/ yzw: 予約。HLSL側は float4
    float pcssParams[4]{};
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

/// @brief TileCullingConstants 定数バッファ（ライトカリングCompute/Object3Dピクセルシェーダー共通）と同レイアウトの構造体
#pragma pack(push, 4)
struct TileCullingConstants {
    Vector2 screenSize{ 0.0f, 0.0f };
    std::uint32_t tileCountX = 0;
    std::uint32_t tileCountY = 0;
    std::uint32_t pointLightCount = 0;
    std::uint32_t spotLightCount = 0;
    std::uint32_t sphereLightCount = 0;
    std::uint32_t discLightCount = 0;
    std::uint32_t rectLightCount = 0;
    std::uint32_t tubeLightCount = 0;
    std::uint32_t boxLightCount = 0;
    std::uint32_t maxLightsPerTile = 0;
    std::uint32_t tileSize = 16;
};
#pragma pack(pop)

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

/// @brief 視錐台の1平面。ワールド座標pが a*p.x+b*p.y+c*p.z+d >= 0 を満たせば視錐台の内側（この平面基準）
struct FrustumPlane {
    float a = 0.0f, b = 0.0f, c = 0.0f, d = 0.0f;
};

/// @brief ビュー射影行列（このエンジンの行ベクトル規約、D3DのNDC z範囲[0,1]）から視錐台の6平面を抽出する
std::array<FrustumPlane, 6> ExtractFrustumPlanes(const Matrix4x4 &viewProjection) {
    const auto &m = viewProjection.m;
    const float c0[4] = { m[0][0], m[1][0], m[2][0], m[3][0] };
    const float c1[4] = { m[0][1], m[1][1], m[2][1], m[3][1] };
    const float c2[4] = { m[0][2], m[1][2], m[2][2], m[3][2] };
    const float c3[4] = { m[0][3], m[1][3], m[2][3], m[3][3] };
    const auto normalize = [](float a, float b, float c, float d) {
        const float len = std::sqrt(a * a + b * b + c * c);
        if (len > 1e-8f) { a /= len; b /= len; c /= len; d /= len; }
        return FrustumPlane{ a, b, c, d };
    };
    std::array<FrustumPlane, 6> planes;
    planes[0] = normalize(c3[0] + c0[0], c3[1] + c0[1], c3[2] + c0[2], c3[3] + c0[3]); // left   (x >= -w)
    planes[1] = normalize(c3[0] - c0[0], c3[1] - c0[1], c3[2] - c0[2], c3[3] - c0[3]); // right  (x <=  w)
    planes[2] = normalize(c3[0] + c1[0], c3[1] + c1[1], c3[2] + c1[2], c3[3] + c1[3]); // bottom (y >= -w)
    planes[3] = normalize(c3[0] - c1[0], c3[1] - c1[1], c3[2] - c1[2], c3[3] - c1[3]); // top    (y <=  w)
    planes[4] = normalize(c2[0], c2[1], c2[2], c2[3]);                                 // near   (D3D: z >= 0)
    planes[5] = normalize(c3[0] - c2[0], c3[1] - c2[1], c3[2] - c2[2], c3[3] - c2[3]); // far    (D3D: z <= w)
    return planes;
}

/// @brief 球が視錐台と交差する可能性があるか（完全に外側であることが確定した場合のみfalse。
///        視錐台の角付近では偽陽性があり得るが偽陰性は無い、カリング用途では安全な近似）
bool SphereIntersectsFrustum(const std::array<FrustumPlane, 6> &planes, const Vector3 &center, float radius) {
    for (const auto &p : planes) {
        const float dist = p.a * center.x + p.b * center.y + p.c * center.z + p.d;
        if (dist < -radius) return false;
    }
    return true;
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

/// @brief 指定の描画先・パイプラインに適用されるPoint/Spot/Directionalライトを収集する
/// @details BindLightBuffersAndShadowMapとProcessLightCullingの両方から使われる（ライトの絞り込み条件は完全に一致させる）
/// @param findShadowIndex ライトが影を生成する場合のシャドウマップスロット番号を返すコールバック（呼び出し側が用意する）
void CollectLightsForTarget(SceneRenderer *sceneRenderer, IRenderTarget *target, const std::string &pipelineName,
    const std::function<std::int32_t(const LightRenderer *)> &findShadowIndex,
    std::vector<PointLightElement> &pointLights,
    std::vector<SpotLightElement> &spotLights,
    std::vector<DirectionalLightElement> &directionalLights,
    std::vector<SphereLightElement> &sphereLights,
    std::vector<DiscLightElement> &discLights,
    std::vector<RectLightElement> &rectLights,
    std::vector<TubeLightElement> &tubeLights,
    std::vector<BoxLightElement> &boxLights) {
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
        } else if (light->GetType() == Light::Type::Sphere) {
            SphereLightElement element;
            element.enabled = light->IsActive() ? 1u : 0u;
            element.color = light->GetColor();
            element.position = lightRenderer->GetWorldPosition();
            element.radius = light->GetRadius();
            element.sourceRadius = light->GetSourceRadius();
            element.intensity = light->GetIntensity();
            element.decay = light->GetDecay();
            element.shadowMapIndex = findShadowIndex(lightRenderer);
            sphereLights.push_back(element);
        } else if (light->GetType() == Light::Type::Disc) {
            DiscLightElement element;
            element.enabled = light->IsActive() ? 1u : 0u;
            element.color = light->GetColor();
            element.position = lightRenderer->GetWorldPosition();
            element.distance = light->GetDistance();
            element.direction = lightRenderer->GetWorldDirection();
            element.sourceRadius = light->GetSourceRadius();
            element.intensity = light->GetIntensity();
            element.decay = light->GetDecay();
            element.shadowMapIndex = findShadowIndex(lightRenderer);
            discLights.push_back(element);
        } else if (light->GetType() == Light::Type::Rect) {
            RectLightElement element;
            element.enabled = light->IsActive() ? 1u : 0u;
            element.color = light->GetColor();
            element.position = lightRenderer->GetWorldPosition();
            element.distance = light->GetDistance();
            element.direction = lightRenderer->GetWorldDirection();
            element.right = lightRenderer->GetWorldRight();
            element.width = light->GetSourceWidth();
            element.up = lightRenderer->GetWorldUp();
            element.height = light->GetSourceHeight();
            element.intensity = light->GetIntensity();
            element.decay = light->GetDecay();
            element.shadowMapIndex = findShadowIndex(lightRenderer);
            rectLights.push_back(element);
        } else if (light->GetType() == Light::Type::Tube) {
            TubeLightElement element;
            element.enabled = light->IsActive() ? 1u : 0u;
            element.color = light->GetColor();
            const Vector3 center = lightRenderer->GetWorldPosition();
            const Vector3 axis = lightRenderer->GetWorldRight();
            const float halfLength = light->GetSourceLength() * 0.5f;
            element.p0 = center - axis * halfLength;
            element.p1 = center + axis * halfLength;
            element.radius = light->GetRadius();
            element.sourceRadius = light->GetSourceRadius();
            element.intensity = light->GetIntensity();
            element.decay = light->GetDecay();
            element.shadowMapIndex = findShadowIndex(lightRenderer);
            tubeLights.push_back(element);
        } else if (light->GetType() == Light::Type::Box) {
            BoxLightElement element;
            element.enabled = light->IsActive() ? 1u : 0u;
            element.color = light->GetColor();
            element.position = lightRenderer->GetWorldPosition();
            element.radius = light->GetRadius();
            element.right = lightRenderer->GetWorldRight();
            element.halfWidth = light->GetSourceWidth() * 0.5f;
            element.up = lightRenderer->GetWorldUp();
            element.halfHeight = light->GetSourceHeight() * 0.5f;
            element.forward = lightRenderer->GetWorldDirection();
            element.halfDepth = light->GetSourceDepth() * 0.5f;
            element.intensity = light->GetIntensity();
            element.decay = light->GetDecay();
            element.shadowMapIndex = findShadowIndex(lightRenderer);
            boxLights.push_back(element);
        }
    }
}

/// @brief 指定の描画先・パイプラインに適用されるカメラの定数バッファを解決する（複数該当する場合は最後に一致したもの）
ConstantBufferResource *ResolveCameraConstantBuffer(SceneRenderer *sceneRenderer, IRenderTarget *target, const std::string &pipelineName) {
    if (auto *editorCameraBuffer = sceneRenderer->GetEditorCameraBuffer(target)) {
        return editorCameraBuffer;
    }
    ConstantBufferResource *result = nullptr;
    for (auto *cameraRenderer : sceneRenderer->GetCameraRenderers()) {
        if (!cameraRenderer || !cameraRenderer->IsActive()) continue;
        if (IsExcludedAsEditorOnly(cameraRenderer, target, sceneRenderer)) continue;
        if (!cameraRenderer->GetPipelineName().empty() && cameraRenderer->GetPipelineName() != pipelineName) continue;
        if (!IsTargetMatch(cameraRenderer->GetTargetObject(), cameraRenderer->GetTargetObjectID().IsValid(), target)) continue;
        if (!cameraRenderer->IsRenderTargetIncluded(target)) continue;
        if (auto *constantBuffer = cameraRenderer->GetConstantBuffer()) {
            result = constantBuffer;
        }
    }
    return result;
}

/// @brief 指定の描画先に適用されるカメラの情報を解決する（AO等、深度からワールド座標を
///        再構成したいポストエフェクト、Outline等、Near/Farの線形化が必要なポストエフェクト用）。
///        ポストエフェクトは特定パイプラインに紐付かないため、ResolveCameraConstantBuffer と
///        異なりパイプライン名による絞り込みは行わない
IPostProcessComponent::CameraInfo ResolveCameraInfoForPostProcess(SceneRenderer *sceneRenderer, IRenderTarget *target) {
    IPostProcessComponent::CameraInfo result;
    if (const auto *editorInfo = sceneRenderer->GetEditorCameraInfo(target)) {
        if (editorInfo->valid) {
            result.valid = true;
            result.viewProjection = editorInfo->viewProjection;
            result.worldPosition = editorInfo->position;
            result.nearClip = editorInfo->nearClip;
            result.farClip = editorInfo->farClip;
            return result;
        }
    }
    for (auto *cameraRenderer : sceneRenderer->GetCameraRenderers()) {
        if (!cameraRenderer || !cameraRenderer->IsActive()) continue;
        if (IsExcludedAsEditorOnly(cameraRenderer, target, sceneRenderer)) continue;
        if (!IsTargetMatch(cameraRenderer->GetTargetObject(), cameraRenderer->GetTargetObjectID().IsValid(), target)) continue;
        if (!cameraRenderer->IsRenderTargetIncluded(target)) continue;
        result.valid = true;
        result.viewProjection = cameraRenderer->GetViewProjectionMatrix();
        result.worldPosition = cameraRenderer->GetWorldPosition();
        result.nearClip = cameraRenderer->GetNearClip();
        result.farClip = cameraRenderer->GetFarClip();
    }
    return result;
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
    if (directXCommon_ && particleComputeCommandSlotIndex_ >= 0) {
        directXCommon_->ReleaseCommandObjects(Passkey<Renderer>{}, particleComputeCommandSlotIndex_);
        particleComputeCommandSlotIndex_ = -1;
        particleComputeCommands_ = nullptr;
    }
    if (directXCommon_ && skinningCommandSlotIndex_ >= 0) {
        directXCommon_->ReleaseCommandObjects(Passkey<Renderer>{}, skinningCommandSlotIndex_);
        skinningCommandSlotIndex_ = -1;
        skinningCommands_ = nullptr;
    }
    if (directXCommon_ && lightCullingCommandSlotIndex_ >= 0) {
        directXCommon_->ReleaseCommandObjects(Passkey<Renderer>{}, lightCullingCommandSlotIndex_);
        lightCullingCommandSlotIndex_ = -1;
        lightCullingCommands_ = nullptr;
    }
}

ID3D12GraphicsCommandList *Renderer::BeginDedicatedComputeCommandList(int &slotIndex, DX12Commands *&commands) {
    if (!directXCommon_) return nullptr;
    if (slotIndex < 0) {
        slotIndex = directXCommon_->AcquireCommandObjects(Passkey<Renderer>{});
        commands = (slotIndex >= 0)
            ? directXCommon_->GetCommandObjects(Passkey<Renderer>{}, slotIndex)
            : nullptr;
    }
    if (!commands) return nullptr;
    return commands->BeginRecord();
}

void Renderer::EndDedicatedComputeCommandList(DX12Commands *commands) {
    if (!commands || !directXCommon_) return;
    if (commands->EndRecord()) {
        directXCommon_->AddRecordCommandList(Passkey<Renderer>{}, commands->GetCommandList());
    }
}

void Renderer::RenderFrame(Passkey<GraphicsEngine>, SceneContext *sceneContext) {
    drawCallCount_ = 0;
    if (!sceneContext || !pipelineManager_) return;

    // Computeシェーダー処理は他の描画パスより先に実行し、結果を後続パスから参照できるようにする
    ProcessComputeShaders(sceneContext);
    // GPUスキニングも描画リスト構築より先に実行し、スキニング結果を描画パスから参照できるようにする
    ProcessSkinning(sceneContext);
    // GPUパーティクルも同様に描画リスト構築より先に実行する
    ProcessGpuParticles(sceneContext);

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

    // Forward+のタイルライトカリング（3D描画で使われる (描画先,パイプライン) の組ごとに実行する）
    ProcessLightCulling(sceneContext, sceneRenderer, drawList);

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

    // ライトカリング専用のコマンドリストへ記録する（ComputeCommandProcessorの共有コマンドリストを
    // 複数フェーズで使い回すと、BeginRecord内のReset()で他フェーズの記録内容が提出前に消えてしまうため）
    auto *commandList = BeginDedicatedComputeCommandList(lightCullingCommandSlotIndex_, lightCullingCommands_);
    if (!commandList) return;

    // 専用コマンドリストはフレームごとにReset()されるため、パイプラインバインド状態は毎回作り直す
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

    EndDedicatedComputeCommandList(lightCullingCommands_);
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
                case Light::Type::Point:
                case Light::Type::Sphere:
                case Light::Type::Tube:
                case Light::Type::Box: estimatedSlices += 6; break;
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
                    (type == Light::Type::Point || type == Light::Type::Sphere || type == Light::Type::Tube || type == Light::Type::Box) ? 6u : 1u;
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
                        job.cascadeBiasScales[c] = (texelWorldSize / (backDistance + radius)) * light->GetShadowBias();
                    }
                } else if (type == Light::Type::Spot || type == Light::Type::Disc || type == Light::Type::Rect) {
                    //--------- Spot: ライト位置からコーン方向への透視投影1面 ---------//
                    //--------- Disc/Rect: 片面（半球）発光を、法線方向への広角(約175度)の単一透視投影で近似する ---------//
                    job.lightType = 1;
                    const Vector3 lightDirection = lightRenderer->GetWorldDirection();
                    const Vector3 lightPosition = lightRenderer->GetWorldPosition();
                    // 外側コーン角の2倍を画角にする（コーン全体を覆う）。Disc/Rectはコーン角の概念が無いため固定の広角を使う
                    const float fovY = (type == Light::Type::Spot)
                        ? std::clamp(light->GetOuterAngle() * 2.0f, 0.05f, 3.1f)
                        : 3.05f;
                    const float nearZ = 0.05f;
                    const float farZ = std::max(nearZ + 0.1f, light->GetDistance());
                    Matrix4x4 lightProjection;
                    lightProjection.MakePerspectiveFovMatrix(fovY, 1.0f, nearZ, farZ);
                    job.viewProjections[0] = makeLightView(lightDirection, lightPosition) * lightProjection;
                    // 深度バイアス係数: シェーダー側でビュー深度wで割ることで、その距離での
                    // 1テクセルのワールドサイズに比例したNDC深度バイアスになる
                    // （透視投影はNDC深度が非線形のため、定数NDCバイアスだと遠方で影が大きく浮いてしまう）
                    job.perspectiveBiasScale = (2.0f * std::tan(fovY * 0.5f) / static_cast<float>(resolution))
                        * (nearZ * farZ / (farZ - nearZ)) * light->GetShadowBias();
                } else {
                    //--------- Point/Sphere: ライト位置からキューブ6面（画角90度）の透視投影 ---------//
                    //--------- Tube: 中心点からのキューブ6面で近似し、遠平面をチューブの半長分拡張する ---------//
                    //--------- Box: 中心点からのキューブ6面で近似し、遠平面をボックスの半対角分拡張する ---------//
                    job.lightType = 2;
                    const Vector3 lightPosition = lightRenderer->GetWorldPosition();
                    const float rangeExtension = (type == Light::Type::Tube) ? (light->GetSourceLength() * 0.5f) :
                        (type == Light::Type::Box) ? Vector3(light->GetSourceWidth(), light->GetSourceHeight(), light->GetSourceDepth()).Length() * 0.5f : 0.0f;
                    const float nearZ = 0.05f;
                    const float farZ = std::max(nearZ + 0.1f, light->GetRadius() + rangeExtension);
                    Matrix4x4 lightProjection;
                    lightProjection.MakePerspectiveFovMatrix(1.5707963f, 1.0f, nearZ, farZ);
                    // 深度バイアス係数（Spotと同様。画角90度なので tan(fov/2) = 1）
                    job.perspectiveBiasScale = (2.0f / static_cast<float>(resolution))
                        * (nearZ * farZ / (farZ - nearZ)) * light->GetShadowBias();
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
        /// @brief 描画するインデックス範囲（サブメッシュ。indexCount==0の場合はメッシュ全体）
        std::uint32_t indexStart = 0;
        std::uint32_t indexCount = 0;
    };
    std::vector<ShadowDrawSource> sources;
    const auto isShadowCastingPipeline = [](const std::string &name) {
        // 3Dオブジェクト描画用パイプラインのみ対象（シャドウマップ用パイプライン自体は除外）
        return name.rfind("Object3D", 0) == 0 && name.rfind("Object3D.ShadowMap", 0) != 0;
    };
    // サブメッシュ（マテリアルごとのインデックス範囲）ごとに1件収集する
    const auto appendShadowSources = [&sources](auto *renderer, RWStructuredBufferResource *skinnedVertexBuffer) {
        const auto &subMeshes = ModelManager::GetModelData(renderer->GetMeshHandle()).GetSubMeshes();
        const size_t subMeshCount = std::max<size_t>(1, subMeshes.size());
        for (size_t subMeshIndex = 0; subMeshIndex < subMeshCount; ++subMeshIndex) {
            ShadowDrawSource source;
            source.meshHandle = renderer->GetMeshHandle();
            source.materialHandle = renderer->GetMaterialHandleAt(subMeshIndex);
            source.worldMatrix = renderer->GetWorldMatrix();
            source.skinnedVertexBuffer = skinnedVertexBuffer;
            if (!subMeshes.empty()) {
                source.indexStart = subMeshes[subMeshIndex].indexStart;
                source.indexCount = subMeshes[subMeshIndex].indexCount;
            }
            sources.push_back(source);
        }
    };
    for (auto *renderer : sceneRenderer->GetMeshRenderers()) {
        if (!renderer || !renderer->IsActive()) continue;
        if (!renderer->GetCastShadows()) continue;
        if (renderer->GetMeshHandle() == ModelManager::kInvalidHandle) continue;
        if (!isShadowCastingPipeline(renderer->GetPipelineName())) continue;
        appendShadowSources(renderer, nullptr);
    }
    for (auto *renderer : sceneRenderer->GetSkinnedMeshRenderers()) {
        if (!renderer || !renderer->IsActive()) continue;
        if (!renderer->GetCastShadows()) continue;
        if (renderer->GetMeshHandle() == ModelManager::kInvalidHandle) continue;
        if (!renderer->HasValidSkinningData()) continue;
        if (!isShadowCastingPipeline(renderer->GetPipelineName())) continue;
        appendShadowSources(renderer, renderer->GetSkinnedVertexBuffer());
    }

    // 同一（メッシュ・サブメッシュ・マテリアル）をまとめてインスタンシング描画できるようにソート
    std::stable_sort(sources.begin(), sources.end(),
        [](const ShadowDrawSource &a, const ShadowDrawSource &b) {
            if (a.skinnedVertexBuffer != b.skinnedVertexBuffer) return a.skinnedVertexBuffer < b.skinnedVertexBuffer;
            if (a.meshHandle != b.meshHandle) return a.meshHandle < b.meshHandle;
            if (a.indexStart != b.indexStart) return a.indexStart < b.indexStart;
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
        /// @brief 描画するインデックス範囲（サブメッシュ。indexCount==0の場合はメッシュ全体）
        std::uint32_t indexStart = 0;
        std::uint32_t indexCount = 0;
        /// @brief バッチ内の全インスタンスを包含するワールド空間の集合境界球（スライス単位カリングに使う）
        Vector3 boundsCenter{ 0.0f, 0.0f, 0.0f };
        float boundsRadius = 0.0f;
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
                   sources[end].skinnedVertexBuffer == first.skinnedVertexBuffer &&
                   sources[end].indexStart == first.indexStart &&
                   sources[end].indexCount == first.indexCount) {
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
            batch.indexStart = first.indexStart;
            batch.indexCount = first.indexCount;

            // ワールド空間の集合境界球を計算する（全スライス共通で1回だけ計算し、スライス単位の
            // フラスタムカリングで「このバッチが完全に視錐台の外側にあるか」を判定するのに使う）
            {
                const auto transformPoint = [](const Matrix4x4 &world, const Vector3 &p) {
                    return Vector3(
                        p.x * world.m[0][0] + p.y * world.m[1][0] + p.z * world.m[2][0] + world.m[3][0],
                        p.x * world.m[0][1] + p.y * world.m[1][1] + p.z * world.m[2][1] + world.m[3][1],
                        p.x * world.m[0][2] + p.y * world.m[1][2] + p.z * world.m[2][2] + world.m[3][2]);
                };
                const auto maxAxisScale = [](const Matrix4x4 &world) {
                    const auto lenSq = [](float x, float y, float z) { return x * x + y * y + z * z; };
                    const float s0 = lenSq(world.m[0][0], world.m[0][1], world.m[0][2]);
                    const float s1 = lenSq(world.m[1][0], world.m[1][1], world.m[1][2]);
                    const float s2 = lenSq(world.m[2][0], world.m[2][1], world.m[2][2]);
                    return std::sqrt(std::max({ s0, s1, s2 }));
                };

                std::vector<Vector3> instanceCenters(instanceCount);
                std::vector<float> instanceRadii(instanceCount);
                Vector3 avgCenter{ 0.0f, 0.0f, 0.0f };
                for (size_t i = begin; i < end; ++i) {
                    const Vector3 center = transformPoint(sources[i].worldMatrix, meshBuffers->boundsCenter);
                    const float radius = meshBuffers->boundsRadius * maxAxisScale(sources[i].worldMatrix);
                    instanceCenters[i - begin] = center;
                    instanceRadii[i - begin] = radius;
                    avgCenter.x += center.x; avgCenter.y += center.y; avgCenter.z += center.z;
                }
                avgCenter.x /= static_cast<float>(instanceCount);
                avgCenter.y /= static_cast<float>(instanceCount);
                avgCenter.z /= static_cast<float>(instanceCount);
                float maxRadius = 0.0f;
                for (std::uint32_t i = 0; i < instanceCount; ++i) {
                    const float dx = instanceCenters[i].x - avgCenter.x;
                    const float dy = instanceCenters[i].y - avgCenter.y;
                    const float dz = instanceCenters[i].z - avgCenter.z;
                    const float dist = std::sqrt(dx * dx + dy * dy + dz * dz) + instanceRadii[i];
                    maxRadius = std::max(maxRadius, dist);
                }
                batch.boundsCenter = avgCenter;
                batch.boundsRadius = maxRadius;
            }

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
                element.rimColor = material->rimColor;
                element.rimPower = material->rimPower;
                element.rimIntensity = material->rimIntensity;
                element.useNormalMap = 0.0f;
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

    //--------- GPUパーティクルの影キャスターを収集する ---------//
    // GPUパーティクルは通常描画（RenderGpuParticles）と同じ考え方で、CPU側にワールド行列を
    // 持たない（gpuInstanceMatrixBuffer_をそのままgTransformationMatricesとしてバインドする）。
    // マテリアルはエミッター全体で1つのため、instanceCount分だけ複製したバッファを用意する
    // （通常のMeshRenderer由来バッチと違い、他のエミッターとまとめてインスタンシングはできない）
    struct PreparedGpuParticleShadowBatch {
        const ResourceContainer::MeshBuffers *meshBuffers = nullptr;
        RWStructuredBufferResource *transformBuffer = nullptr;
        StructuredBufferResource *materialBuffer = nullptr;
        std::uint32_t textureHandle = TextureManager::kInvalidHandle;
        SamplerManager::SamplerHandle samplerHandle = SamplerManager::kInvalidHandle;
        std::uint32_t instanceCount = 0;
    };
    std::vector<PreparedGpuParticleShadowBatch> gpuParticleBatches;
    {
        std::uint32_t emitterIndex = 0;
        for (auto *emitter : sceneRenderer->GetGpuParticleEmitters()) {
            if (!emitter || !emitter->IsActive() || !emitter->IsGPUSimulation() || !emitter->GetCastShadows()) continue;

            const auto meshHandle = emitter->GetMeshHandle();
            if (meshHandle == ModelManager::kInvalidHandle) continue;
            const auto *meshBuffers = resourceContainer_->GetOrCreateMeshBuffers(meshHandle);
            if (!meshBuffers || !meshBuffers->vertexBuffer || !meshBuffers->indexBuffer) continue;

            auto *instanceMatrixBuffer = emitter->GetGpuInstanceMatrixBuffer(Passkey<Renderer>{});
            if (!instanceMatrixBuffer) continue;
            const std::uint32_t instanceCount = emitter->GetGpuParticleCapacity(Passkey<Renderer>{});
            if (instanceCount == 0) continue;

            PreparedGpuParticleShadowBatch batch;
            batch.meshBuffers = meshBuffers;
            batch.transformBuffer = instanceMatrixBuffer;
            batch.instanceCount = instanceCount;

            auto *material = MaterialManager::GetMaterial(emitter->GetMaterialHandle());
            if (material) material->ResolveTextureHandles();
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
                element.rimColor = material->rimColor;
                element.rimPower = material->rimPower;
                element.rimIntensity = material->rimIntensity;
                element.useNormalMap = 0.0f;
                batch.textureHandle = material->textureHandle;
                batch.samplerHandle = material->samplerHandle;
            }
            char key[64];
            std::snprintf(key, sizeof(key), "ShadowPass|gpuParticle|%u|material", emitterIndex);
            batch.materialBuffer = resourceContainer_->GetOrCreateStructuredBuffer(key, sizeof(MaterialElement), instanceCount);
            if (batch.materialBuffer) {
                if (auto *mapped = static_cast<MaterialElement *>(batch.materialBuffer->Map())) {
                    for (std::uint32_t i = 0; i < instanceCount; ++i) {
                        mapped[i] = element;
                    }
                }
            }

            if (batch.materialBuffer) {
                gpuParticleBatches.push_back(batch);
                ++emitterIndex;
            }
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

            // このスライスの視錐台を抽出し、完全に外側にあるバッチは描画自体をスキップする
            const auto frustumPlanes = ExtractFrustumPlanes(job.viewProjections[s]);

            for (const auto &batch : batches) {
                if (!SphereIntersectsFrustum(frustumPlanes, batch.boundsCenter, batch.boundsRadius)) {
                    continue;
                }
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
                const std::uint32_t drawIndexCount = batch.indexCount > 0 ? batch.indexCount : batch.meshBuffers->indexCount;
                commandList->DrawIndexedInstanced(drawIndexCount, batch.instanceCount, batch.indexStart, 0, 0);
                ++drawCallCount_;
            }

            for (const auto &batch : gpuParticleBatches) {
                shaderBinder.Bind("Vertex:gCamera3D", cameraBuffer);
                batch.transformBuffer->SetCommandList(commandList);
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

                pipelineBinder.SetVertexBuffer(batch.meshBuffers->vertexBuffer.get(), sizeof(ResourceContainer::MeshVertex));
                pipelineBinder.SetIndexBuffer(batch.meshBuffers->indexBuffer.get());
                // 死んでいるパーティクルはgpuInstanceMatrixBuffer_内でスケール0の行列になっているため、
                // 追加のカリング無しでcapacity件（常にMax Particles分）そのままインスタンス描画する
                commandList->DrawIndexedInstanced(batch.meshBuffers->indexCount, batch.instanceCount, 0, 0, 0);
                ++drawCallCount_;
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
    auto *sceneRenderer = sceneContext->GetComponent<SceneRenderer>();
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
            RenderPostProcess(buffer, pipelineBinder, object.get(), sceneRenderer);
            buffer->EndDraw();
            // 今フレームの最終確定SRVをビューア用に記録する（詳細はScreenBuffer::SetPreviewSrvHandle参照）
            buffer->SetPreviewSrvHandle(Passkey<Renderer>{}, buffer->GetSrvHandle());
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

    // 同一パイプライン・描画先が連続する間はカメラ・ライトの再バインドを省略するための共有キャッシュ
    // （DrawBatchのループ・RenderTextRenderers・RenderGpuParticlesの全体で1つを使い回す）
    CameraLightsBindCache lightsCache;

    // 同一（パイプライン・メッシュ・サブメッシュ・マテリアル）の連続範囲をバッチとしてまとめて描画
    size_t begin = 0;
    while (begin < entries.size()) {
        const auto &first = entries[begin];
        size_t end = begin;
        while (end < entries.size()) {
            const auto &other = entries[end];
            if (other.pipelineName != first.pipelineName ||
                other.meshHandle != first.meshHandle ||
                other.materialHandle != first.materialHandle ||
                other.indexStart != first.indexStart ||
                other.indexCount != first.indexCount ||
                other.skinnedVertexBuffer != first.skinnedVertexBuffer) {
                break;
            }
            ++end;
        }

        DrawBatch(target, pipelineBinder, entries.subspan(begin, end - begin), sceneRenderer, lightsCache);
        begin = end;
    }

    // TextRenderer（文字ごとにアトラス内UVが異なるため通常のバッチには乗らない）は専用パスで描画する
    RenderTextRenderers(target, pipelineBinder, sceneRenderer, lightsCache);

    // GPU Simulation有効なParticleSystem2D/3D（ProcessGpuParticlesが結果を書き込み済み）も専用パスで描画する
    RenderGpuParticles(target, pipelineBinder, sceneRenderer, lightsCache);

    // ScreenBuffer の場合は所有オブジェクトのポストプロセスを適用
    if (target->GetRenderTargetKind() == RenderTargetKind::ScreenBuffer) {
        auto *screenBuffer = static_cast<ScreenBuffer *>(target);
        // エディター用描画先の場合、デバッグ表示（グリッド・当たり判定）をポストプロセスより先に描画する
        RenderEditorDebugOverlay(screenBuffer, pipelineBinder, sceneRenderer);
        RenderPostProcess(screenBuffer, pipelineBinder, sceneRenderer->GetTargetOwner(target), sceneRenderer);
    }

    // ウィンドウのコマンドリストはこの後 ImGui 等の描画にも使われるため、
    // 描画終了処理はスワップチェーン側（DirectXCommon::EndDraw）に任せる
    if (target->GetRenderTargetKind() != RenderTargetKind::Window) {
        target->EndDraw();
        if (target->GetRenderTargetKind() == RenderTargetKind::ScreenBuffer) {
            // 今フレームの最終確定SRVをビューア用に記録する（詳細はScreenBuffer::SetPreviewSrvHandle参照）
            auto *screenBuffer = static_cast<ScreenBuffer *>(target);
            screenBuffer->SetPreviewSrvHandle(Passkey<Renderer>{}, screenBuffer->GetSrvHandle());
        }
    }
}

void Renderer::DrawBatch(IRenderTarget *target,
    PipelineBinder &pipelineBinder,
    std::span<const SceneRenderer::DrawEntry> batch,
    SceneRenderer *sceneRenderer,
    CameraLightsBindCache &lightsCache) {
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

    // カメラ・ライトの定数バッファバインド（直前と同じパイプラインのままなら内部でスキップされる）
    BindCameraAndLights(commandList, target, pipelineName, sceneRenderer, pipelineBinder, lightsCache);

    const std::uint32_t instanceCount = static_cast<std::uint32_t>(batch.size());

    // ワールド行列のインスタンスバッファ
    {
        // サブメッシュ（同一メッシュ・同一マテリアルでもインデックス範囲が異なる）ごとに
        // 別バッファを使うよう、キーにインデックス範囲を含める
        char transformSuffix[48];
        std::snprintf(transformSuffix, sizeof(transformSuffix), "transform|%u", first.indexStart);
        auto key = MakeBatchKey(target, pipelineName, first.meshHandle, first.materialHandle, transformSuffix);
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
        // 動かない静的オブジェクトのみのバッチでは前フレームと内容が完全に一致するため、
        // GetOrUpdateStructuredBufferが内容比較によりMap+memcpyを省略する
        std::vector<Matrix4x4> transforms(instanceCount);
        for (size_t i = 0; i < batch.size(); ++i) {
            transforms[i] = batch[i].worldMatrix;
        }
        auto *instanceBuffer = resourceContainer_->GetOrUpdateStructuredBuffer(key, sizeof(Matrix4x4), instanceCount, transforms.data());
        if (!instanceBuffer) return;
        shaderBinder.Bind("Vertex:gTransformationMatrices", instanceBuffer);
    }

    // マテリアルの構造化バッファ（シェーダーはインスタンスIDで参照するため個数分並べる）
    {
        auto *material = MaterialManager::GetMaterial(first.materialHandle);
        if (material) {
            // 読み込み時に未解決だったテクスチャハンドルの解決を試みる
            material->ResolveTextureHandles();
        }

        char materialSuffix[48];
        std::snprintf(materialSuffix, sizeof(materialSuffix), "material|%u", first.indexStart);
        auto key = MakeBatchKey(target, pipelineName, first.meshHandle, first.materialHandle, materialSuffix);
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
            std::vector<Material2DElement> elements(instanceCount, element);
            auto *materialBuffer = resourceContainer_->GetOrUpdateStructuredBuffer(key, sizeof(Material2DElement), instanceCount, elements.data());
            if (materialBuffer) {
                shaderBinder.Bind("Pixel:gMaterials", materialBuffer);
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
                element.rimColor = material->rimColor;
                element.rimPower = material->rimPower;
                element.rimIntensity = material->rimIntensity;
                element.useNormalMap = (material->normalMapHandle != TextureManager::kInvalidHandle) ? 1.0f : 0.0f;
            }
            std::vector<MaterialElement> elements(instanceCount, element);
            auto *materialBuffer = resourceContainer_->GetOrUpdateStructuredBuffer(key, sizeof(MaterialElement), instanceCount, elements.data());
            if (materialBuffer) {
                shaderBinder.Bind("Pixel:gMaterials", materialBuffer);
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
        if (material && material->normalMapHandle != TextureManager::kInvalidHandle) {
            TextureManager::BindTexture(&shaderBinder, "Pixel:gNormalMap", material->normalMapHandle);
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
    // サブメッシュ範囲が指定されている場合はその範囲のみ描画する（indexCount==0はメッシュ全体）
    const std::uint32_t drawIndexCount = first.indexCount > 0 ? first.indexCount : meshBuffers->indexCount;
    commandList->DrawIndexedInstanced(drawIndexCount, instanceCount, first.indexStart, 0, 0);
    ++drawCallCount_;
}

void Renderer::RenderTextRenderers(IRenderTarget *target, PipelineBinder &pipelineBinder, SceneRenderer *sceneRenderer,
    CameraLightsBindCache &lightsCache) {
    if (!target || !sceneRenderer) return;

    //--------- このターゲットに適用されるTextRendererを収集する（CollectSortableEntriesと同じフィルタ条件） ---------//
    struct TextTargetEntry {
        TextRenderer *renderer = nullptr;
        std::string pipelineName;
    };
    std::vector<TextTargetEntry> applicable;
    std::vector<IRenderTarget *> collectedTargets;
    auto *editorTarget = sceneRenderer->GetEditorTarget();

    for (auto *renderer : sceneRenderer->GetTextRenderers()) {
        if (!renderer || !renderer->IsActive()) continue;
        const std::string &pipelineName = renderer->GetPipelineName();
        if (pipelineName.empty() || !pipelineManager_->HasPipeline(pipelineName)) continue;

        if (IsExcludedAsEditorOnly(renderer, target, sceneRenderer)) continue;

        auto *targetObject = renderer->GetTargetObject();
        SceneRenderer::CollectRenderTargets(targetObject, collectedTargets);
        if (editorTarget && editorTarget->IsRenderTargetAvailable()) {
            collectedTargets.push_back(editorTarget);
        }

        bool matches = false;
        for (auto *candidate : collectedTargets) {
            if (candidate != target || !target->IsRenderTargetAvailable()) continue;
            if (target != editorTarget && !renderer->IsRenderTargetIncluded(target)) continue;
            matches = true;
            break;
        }
        if (!matches) continue;

        applicable.push_back(TextTargetEntry{ renderer, pipelineName });
    }
    if (applicable.empty()) return;

    //--------- (パイプライン名, フォントハンドル) ごとにグループ化して文字インスタンスをまとめる ---------//
    std::map<std::pair<std::string, FontManager::FontHandle>, std::vector<TextRenderer::RenderCharacterInstance>> groups;
    for (const auto &entry : applicable) {
        const auto fontHandle = entry.renderer->GetFontHandle();
        if (fontHandle == FontManager::kInvalidHandle) continue;
        auto instances = entry.renderer->GetRenderInstances();
        if (instances.empty()) continue;
        auto &bucket = groups[std::make_pair(entry.pipelineName, fontHandle)];
        bucket.insert(bucket.end(), instances.begin(), instances.end());
    }
    if (groups.empty()) return;

    static const ModelManager::ModelHandle kRect2DMeshHandle = ModelManager::GetModelHandleFromAssetPath("PrimitiveMesh-Rect2D");
    const auto *meshBuffers = resourceContainer_->GetOrCreateMeshBuffers(kRect2DMeshHandle);
    if (!meshBuffers || !meshBuffers->vertexBuffer || !meshBuffers->indexBuffer) return;

    auto *commandList = target->GetCommandList();

    for (const auto &[key, instances] : groups) {
        const std::string &pipelineName = key.first;
        const FontManager::FontHandle fontHandle = key.second;
        if (instances.empty()) continue;

        pipelineBinder.UsePipeline(pipelineName);
        auto &shaderBinder = pipelineManager_->GetShaderVariableBinder(Passkey<Renderer>{}, pipelineName);
        shaderBinder.SetCommandList(commandList);

        // カメラの定数バッファバインド（ライト関連バッファも一緒にバインドされるが、
        // Text2DのシェーダーはgPointLights等を参照しないため無害。Object2D系の描画と同じ扱い）
        BindCameraAndLights(commandList, target, pipelineName, sceneRenderer, pipelineBinder, lightsCache);

        const std::uint32_t instanceCount = static_cast<std::uint32_t>(instances.size());

        // ワールド行列のインスタンスバッファ
        {
            auto key2 = MakeBatchKey(target, pipelineName, kRect2DMeshHandle, fontHandle, "text_transform");
            auto *instanceBuffer = resourceContainer_->GetOrCreateStructuredBuffer(key2, sizeof(Matrix4x4), instanceCount);
            if (!instanceBuffer) continue;
            auto *mapped = static_cast<Matrix4x4 *>(instanceBuffer->Map());
            if (!mapped) continue;
            for (std::uint32_t i = 0; i < instanceCount; ++i) {
                mapped[i] = instances[i].worldMatrix;
            }
            shaderBinder.Bind("Vertex:gTransformationMatrices", instanceBuffer);
        }

        // 文字ごとの色・UV矩形・SDFパラメータ
        {
            auto key2 = MakeBatchKey(target, pipelineName, kRect2DMeshHandle, fontHandle, "text_material");
            auto *materialBuffer = resourceContainer_->GetOrCreateStructuredBuffer(key2, sizeof(TextCharacterElement), instanceCount);
            if (!materialBuffer) continue;
            auto *mapped = static_cast<TextCharacterElement *>(materialBuffer->Map());
            if (!mapped) continue;
            for (std::uint32_t i = 0; i < instanceCount; ++i) {
                const auto &src = instances[i];
                TextCharacterElement dst;
                dst.color = src.color;
                dst.uvRect = Vector4(src.u0, src.v0, src.u1, src.v1);
                dst.boldWeight = src.boldWeight;
                mapped[i] = dst;
            }
            shaderBinder.Bind("Pixel:gMaterials", materialBuffer);
        }

        TextureManager::BindTexture(&shaderBinder, "Pixel:gTexture", FontManager::GetAtlasTextureHandle(fontHandle));
        SamplerManager::BindSampler(&shaderBinder, "Pixel:gSampler", DefaultSampler::LinearWrap);

        pipelineBinder.SetVertexBuffer(meshBuffers->vertexBuffer.get(), sizeof(ResourceContainer::MeshVertex));
        pipelineBinder.SetIndexBuffer(meshBuffers->indexBuffer.get());
        commandList->DrawIndexedInstanced(meshBuffers->indexCount, instanceCount, 0, 0, 0);
        ++drawCallCount_;
    }
}

void Renderer::RenderGpuParticles(IRenderTarget *target, PipelineBinder &pipelineBinder, SceneRenderer *sceneRenderer,
    CameraLightsBindCache &lightsCache) {
    if (!target || !sceneRenderer) return;
    const auto &emitters = sceneRenderer->GetGpuParticleEmitters();
    if (emitters.empty()) return;

    auto *editorTarget = sceneRenderer->GetEditorTarget();
    auto *commandList = target->GetCommandList();

    for (auto *emitter : emitters) {
        if (!emitter || !emitter->IsActive()) continue;
        const std::string &pipelineName = emitter->GetPipelineName();
        if (pipelineName.empty() || !pipelineManager_->HasPipeline(pipelineName)) continue;
        if (IsExcludedAsEditorOnly(emitter, target, sceneRenderer)) continue;

        std::vector<IRenderTarget *> collectedTargets;
        auto *targetObject = emitter->GetTargetObject();
        SceneRenderer::CollectRenderTargets(targetObject, collectedTargets);
        if (editorTarget && editorTarget->IsRenderTargetAvailable()) {
            collectedTargets.push_back(editorTarget);
        }
        bool matches = false;
        for (auto *candidate : collectedTargets) {
            if (candidate != target || !target->IsRenderTargetAvailable()) continue;
            if (target != editorTarget && !emitter->IsRenderTargetIncluded(target)) continue;
            matches = true;
            break;
        }
        if (!matches) continue;

        const auto meshHandle = emitter->GetMeshHandle();
        if (meshHandle == ModelManager::kInvalidHandle) continue;
        const auto *meshBuffers = resourceContainer_->GetOrCreateMeshBuffers(meshHandle);
        if (!meshBuffers || !meshBuffers->vertexBuffer || !meshBuffers->indexBuffer) continue;

        auto *instanceMatrixBuffer = emitter->GetGpuInstanceMatrixBuffer(Passkey<Renderer>{});
        if (!instanceMatrixBuffer) continue;
        const std::uint32_t instanceCount = emitter->GetGpuParticleCapacity(Passkey<Renderer>{});
        if (instanceCount == 0) continue;

        const bool isObject2D = pipelineName.rfind("Object2D", 0) == 0;

        pipelineBinder.UsePipeline(pipelineName);
        auto &shaderBinder = pipelineManager_->GetShaderVariableBinder(Passkey<Renderer>{}, pipelineName);
        shaderBinder.SetCommandList(commandList);

        BindCameraAndLights(commandList, target, pipelineName, sceneRenderer, pipelineBinder, lightsCache);

        instanceMatrixBuffer->SetCommandList(commandList);
        shaderBinder.Bind("Vertex:gTransformationMatrices", instanceMatrixBuffer);

        // マテリアルはDrawBatchと同じくバッチ全体で1つ（パーティクルごとの色は無し）
        {
            const auto materialHandle = emitter->GetMaterialHandle();
            auto *material = MaterialManager::GetMaterial(materialHandle);
            if (material) material->ResolveTextureHandles();

            auto key = MakeBatchKey(target, pipelineName, meshHandle, materialHandle, "gpu_particle_material");
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
                        for (std::uint32_t i = 0; i < instanceCount; ++i) mapped[i] = element;
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
                    element.rimColor = material->rimColor;
                    element.rimPower = material->rimPower;
                    element.rimIntensity = material->rimIntensity;
                    element.useNormalMap = (material->normalMapHandle != TextureManager::kInvalidHandle) ? 1.0f : 0.0f;
                }
                auto *materialBuffer = resourceContainer_->GetOrCreateStructuredBuffer(key, sizeof(MaterialElement), instanceCount);
                if (materialBuffer) {
                    auto *mapped = static_cast<MaterialElement *>(materialBuffer->Map());
                    if (mapped) {
                        for (std::uint32_t i = 0; i < instanceCount; ++i) mapped[i] = element;
                        shaderBinder.Bind("Pixel:gMaterials", materialBuffer);
                    }
                }
            }

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
            if (material && material->normalMapHandle != TextureManager::kInvalidHandle) {
                TextureManager::BindTexture(&shaderBinder, "Pixel:gNormalMap", material->normalMapHandle);
            }
            if (material && material->samplerHandle != SamplerManager::kInvalidHandle) {
                SamplerManager::BindSampler(&shaderBinder, "Pixel:gSampler", material->samplerHandle);
            } else {
                SamplerManager::BindSampler(&shaderBinder, "Pixel:gSampler", DefaultSampler::LinearWrap);
            }
        }

        pipelineBinder.SetVertexBuffer(meshBuffers->vertexBuffer.get(), sizeof(ResourceContainer::MeshVertex));
        pipelineBinder.SetIndexBuffer(meshBuffers->indexBuffer.get());
        commandList->DrawIndexedInstanced(meshBuffers->indexCount, instanceCount, 0, 0, 0);
        ++drawCallCount_;
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
    EmptyObject *ownerObject,
    SceneRenderer *sceneRenderer) {
    if (!screenBuffer || !ownerObject) return;

    auto postProcessComponents = ownerObject->GetComponents<IPostProcessComponent>();
    if (postProcessComponents.empty()) return;

    auto *commandList = screenBuffer->GetCommandList();
    if (!commandList) return;

    // このスクリーンバッファへ描画したカメラの情報（AOの座標再構成、Outlineの深度線形化等に使う）
    IPostProcessComponent::CameraInfo cameraInfo;
    if (sceneRenderer) {
        cameraInfo = ResolveCameraInfoForPostProcess(sceneRenderer, screenBuffer);
    }

    for (auto *component : postProcessComponents) {
        if (!component || !component->IsActive()) continue;
        // 除外設定されているスクリーンバッファには適用しない
        if (!component->IsScreenBufferIncluded(screenBuffer)) continue;

        // ユーザーがカメラ設定（Near/Far等）を手動で複製せずに済むよう、カメラ情報を先に注入する
        component->SetCameraInfoInterface(Passkey<Renderer>{}, cameraInfo);

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
