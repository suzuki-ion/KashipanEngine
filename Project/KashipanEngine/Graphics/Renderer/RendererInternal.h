#pragma once

#include "Renderer.h"

#include <cstddef>
#include <cstdio>
#include <algorithm>
#include <array>
#include <cmath>
#include <cstring>
#include <functional>
#include <map>
#include <utility>
#include <vector>

#include "Assets/MaterialManager.h"
#include "Assets/SamplerManager.h"
#include "Assets/TextureManager.h"
#include "Assets/TextureRef.h"
#include "Assets/TextureCubeRef.h"
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
#include "Assets/ModelManager.h"
#include "Graphics/Resources/RWStructuredBufferResource.h"
#include "Graphics/Resources/StructuredBufferResource.h"
#include "Objects/EmptyObject.h"
#include "Objects/Components/Transform.h"
#include "Scene/Components/Compute/SceneComputeProcessor.h"
#include "Scene/SceneContext.h"
#include "Utilities/TimeUtils.h"

namespace KashipanEngine::RendererInternal {

/// @brief RenderMultiPassDither（Renderer::RenderMultiPassDither）が使うスクラッチ/蓄積用GPUリソース一式。
/// @details 描画先（ScreenBuffer）ごとに個別に保持する。Renderer全体で1組だけ共有する実装だと、
///          サイズの異なる複数のScreenBufferを同一フレーム内で処理した場合（例:
///          エディターのSceneView用ScreenBufferはパネルの表示領域に毎フレーム合わせてリサイズされる
///          ため、固定解像度のゲーム用ScreenBufferとまずサイズが一致しない）、片方向けにリソースを
///          作り直した瞬間、もう片方が同一フレーム内で既にコマンドリストへ積んだ描画コマンド
///          （まだExecuteCommandLists/GPU実行前）が参照しているリソース・ディスクリプタを
///          破棄してしまう。IGraphicsResourceの解放はGPUフェンス待ちを行わず即座にCOM解放＆
///          ディスクリプタスロットを再利用可能にするため、これが原因でGPU側が無効な
///          ScreenBuffer関連リソースを参照し、SwapChain::Presentの失敗という形でクラッシュしていた。
struct MultiPassDitherScratchSet {
    /// @brief 1パス分の描画結果を書き込むスクラッチカラー（毎パス透明クリアして使い回す）
    std::unique_ptr<RenderTargetResource> scratchColor;
    std::unique_ptr<ShaderResourceResource> scratchColorSrv;
    /// @brief Nパス分を1/N重みで加算合成した蓄積カラー（フレームの最初に透明クリア）
    std::unique_ptr<RenderTargetResource> accumColor;
    std::unique_ptr<ShaderResourceResource> accumColorSrv;
    /// @brief オーナーの深度（不透明・通常ディザ描画済み）のスナップショット
    std::unique_ptr<DepthStencilResource> depthSnapshot;
    /// @brief 毎パス、上記スナップショットから複製して使う作業用深度
    std::unique_ptr<DepthStencilResource> scratchDepth;
};

// gMaterials（Object3D/Object2D/Text2D/Text3D）はBuildMaterialElementBytes（本ファイル下部）で
// パイプラインのMaterialLayoutに従って汎用的にパックするため、専用の固定構造体は持たない
// （Text用のinstanceColor/uvRect/boldWeight/アウトライン値、および旧シェーダー互換の
// characterColorもDrawBatch側で書き込まれる）

/// @brief gPointLights 構造化バッファと同レイアウトの構造体
#pragma pack(push, 4)
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

/// @brief TimeConstants 定数バッファ（Object2D等の時間ベース演出用）と同レイアウトの構造体
struct TimeConstantsData {
    float time = 0.0f;
    float deltaTime = 0.0f;
    float padding[2]{};
};

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
inline SamplerManager::SamplerHandle GetShadowSamplerCmpHandle() {
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
inline std::array<FrustumPlane, 6> ExtractFrustumPlanes(const Matrix4x4 &viewProjection) {
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
inline bool SphereIntersectsFrustum(const std::array<FrustumPlane, 6> &planes, const Vector3 &center, float radius) {
    for (const auto &p : planes) {
        const float dist = p.a * center.x + p.b * center.y + p.c * center.z + p.d;
        if (dist < -radius) return false;
    }
    return true;
}

/// @brief 既にパック済みの1要素バイト列へ、名前指定で1フィールドだけ書き込む（値の型がレイアウト上のフィールドより
///        大きい場合はフィールドのサイズに切り詰める。該当フィールドが無いシェーダーでは何もしない）
inline void WriteMaterialFieldRaw(const PipelineInfo &pipelineInfo, std::byte *elementBytes, std::uint32_t elementByteSize,
    const std::string &name, const void *src, std::uint32_t srcSize) {
    const auto *field = pipelineInfo.GetMaterialLayout().Find(name);
    if (!field) return;
    std::uint32_t copySize = std::min(srcSize, field->byteSize);
    if (static_cast<std::uint64_t>(field->byteOffset) + copySize <= elementByteSize) {
        std::memcpy(elementBytes + field->byteOffset, src, copySize);
    }
}

/// @brief WriteMaterialFieldRaw の型付きラッパー
template <typename T>
inline void WriteMaterialField(const PipelineInfo &pipelineInfo, std::byte *elementBytes, std::uint32_t elementByteSize,
    const std::string &name, const T &value) {
    WriteMaterialFieldRaw(pipelineInfo, elementBytes, elementByteSize, name, &value, static_cast<std::uint32_t>(sizeof(T)));
}

/// @brief Materialの固定フィールド・extraParametersを、パイプラインのPixelシェーダーが定義する
///        struct Material のバイトレイアウト（PipelineInfo::GetMaterialLayout）に従ってパックする（1要素分）。
/// @details レイアウトに存在しないフィールドは黙ってスキップするため、Object3D(16フィールド)・
///          Object2D(4フィールド)・Velocity(6フィールド)等、異なるMaterial定義を持つシェーダーを
///          同一のロジックで扱える。instanceColor等インスタンス単位の値は含まないため、
///          呼び出し側で WriteMaterialField を使って個別に上書きすること
inline std::vector<std::byte> BuildMaterialElementBytes(const PipelineInfo &pipelineInfo, const MaterialManager::Material *material) {
    const auto &layout = pipelineInfo.GetMaterialLayout();
    std::vector<std::byte> bytes(layout.totalByteSize, std::byte{ 0 });
    if (bytes.empty()) return bytes;

    // マテリアルが解決できない場合（無効なマテリアル名の参照等）でも全フィールドがゼロ埋めのまま
    // 描画されてしまう（color.aが0になり不可視化する）のを避けるため、安全な既定値で代替する
    static const MaterialManager::Material kFallbackMaterial{};
    if (!material) material = &kFallbackMaterial;

    auto writeFloat = [&](const char *name, float value) {
        WriteMaterialField(pipelineInfo, bytes.data(), static_cast<std::uint32_t>(bytes.size()), name, value);
    };
    auto writeVector4 = [&](const char *name, const Vector4 &value) {
        WriteMaterialField(pipelineInfo, bytes.data(), static_cast<std::uint32_t>(bytes.size()), name, value);
    };
    auto writeMatrix4x4 = [&](const char *name, const Matrix4x4 &value) {
        WriteMaterialField(pipelineInfo, bytes.data(), static_cast<std::uint32_t>(bytes.size()), name, value);
    };

    writeFloat("enableLighting", material->enableLighting ? 1.0f : 0.0f);
    writeFloat("enableShadowMapProjection", material->enableShadowMapProjection ? 1.0f : 0.0f);
    writeFloat("useTexture", (material->textureHandle != TextureManager::kInvalidHandle) ? 1.0f : 0.0f);
    writeVector4("color", material->color);
    writeMatrix4x4("uvTransform", material->uvTransform);
    writeFloat("shininess", material->shininess);
    writeVector4("specularColor", material->specularColor);
    writeVector4("rimColor", material->rimColor);
    writeFloat("rimPower", material->rimPower);
    writeFloat("rimIntensity", material->rimIntensity);

    // カスタムシェーダー固有の追加パラメータ（レイアウトに同名フィールドが無ければ黙って無視される）
    for (const auto &[name, value] : material->extraParameters) {
        if (const auto *asFloat = value.AnyCastPtr<float>()) {
            writeFloat(name.c_str(), *asFloat);
        } else if (const auto *asVec2 = value.AnyCastPtr<Vector2>()) {
            WriteMaterialField(pipelineInfo, bytes.data(), static_cast<std::uint32_t>(bytes.size()), name, *asVec2);
        } else if (const auto *asVec3 = value.AnyCastPtr<Vector3>()) {
            WriteMaterialField(pipelineInfo, bytes.data(), static_cast<std::uint32_t>(bytes.size()), name, *asVec3);
        } else if (const auto *asVec4 = value.AnyCastPtr<Vector4>()) {
            writeVector4(name.c_str(), *asVec4);
        } else if (const auto *asColor = value.AnyCastPtr<Color>()) {
            // Colorはr,g,b,aの4 floatでVector4と同一レイアウトのため、そのままGPUへ書き込める
            WriteMaterialField(pipelineInfo, bytes.data(), static_cast<std::uint32_t>(bytes.size()), name, *asColor);
        } else if (const auto *asBool = value.AnyCastPtr<bool>()) {
            writeFloat(name.c_str(), *asBool ? 1.0f : 0.0f);
        } else if (const auto *asInt = value.AnyCastPtr<std::int32_t>()) {
            WriteMaterialField(pipelineInfo, bytes.data(), static_cast<std::uint32_t>(bytes.size()), name, *asInt);
        }
    }

    return bytes;
}

/// @brief extraParametersのTextureRef/TextureCubeRef型エントリを、キー名をそのままシェーダー変数名として
///        毎描画バインドする。gTexture等ごく一部の固定スロットとは別に、各シェーダーモジュールが独自に宣言する
///        任意のTexture2D/TextureCubeスロットへ対応するためのもの。該当スロットを持たないパイプラインでは
///        ShaderVariableBinder::Bindが黙ってfalseを返すだけなので無害。未設定/未解決の場合はバインドを
///        スキップする（フォールバックしない）
inline void BindExtraTextureParameters(ShaderVariableBinder *shaderBinder, const MaterialManager::Material *material) {
    if (!material) return;
    for (const auto &[name, value] : material->extraParameters) {
        if (const auto *asTextureRef = value.AnyCastPtr<TextureRef>()) {
            if (asTextureRef->assetPath.empty()) continue;
            const auto handle = TextureManager::GetTextureFromAssetPath(asTextureRef->assetPath);
            if (handle != TextureManager::kInvalidHandle) {
                TextureManager::BindTexture(shaderBinder, "Pixel:" + name, handle);
            }
        } else if (const auto *asTextureCubeRef = value.AnyCastPtr<TextureCubeRef>()) {
            if (asTextureCubeRef->assetPath.empty()) continue;
            const auto handle = TextureManager::GetTextureFromAssetPath(asTextureCubeRef->assetPath);
            if (handle != TextureManager::kInvalidHandle) {
                TextureManager::BindTexture(shaderBinder, "Pixel:" + name, handle);
            }
        }
    }
}

/// @brief バッファキャッシュキー生成（描画先＋パイプライン＋メッシュ＋マテリアルでバッチを識別）
/// @details usageは呼び出し元によっては短い定数より長くなり得るため、切り詰められて
///          別グループ同士のキーが衝突しないよう十分な余裕を持たせている
inline std::string MakeBatchKey(const void *target, const std::string &pipelineName,
    std::uint32_t meshHandle, std::uint32_t materialHandle, const char *usage) {
    char buffer[192];
    std::snprintf(buffer, sizeof(buffer), "%p|%u|%u|%s|", target, meshHandle, materialHandle, usage);
    return std::string(buffer) + pipelineName;
}

/// @brief ComputeShaderProcessing::UAVTextureBindRequirement::formatKind から DXGI_FORMAT へ変換
inline DXGI_FORMAT UAVFormatFromKind(int formatKind) {
    switch (formatKind) {
        case 1: return DXGI_FORMAT_R32_FLOAT;
        case 2: return DXGI_FORMAT_R32G32B32A32_FLOAT;
        default: return DXGI_FORMAT_R8G8B8A8_UNORM;
    }
}

/// @brief 指定の描画先が対象オブジェクトの描画先に含まれるか（未指定の場合は全描画先に適用）
/// @details MeshRenderer/SpriteRenderer等の描画エントリ収集側は、targetObjectIDの指定に
///          関わらずeditorTarget（シーンエディターのプレビュー用描画先）へも常に描画エントリを
///          追加する。カメラ・ライト側がここで一致判定せずeditorTargetを除外してしまうと、
///          描画エントリ自体は生成されるのにカメラ・ライトだけバインドされないまま
///          Drawされ、ルート引数未初期化としてGPUベース検証に検出されクラッシュする
///          （Object2D/Text2DのgCamera2D等で発生）。描画エントリ側と対称になるよう、
///          editorTargetは常にマッチ扱いにする
inline bool IsTargetMatch(EmptyObject *targetObject, bool hasTargetSpecified, IRenderTarget *target, const SceneRenderer *sceneRenderer) {
    if (!hasTargetSpecified) return true;
    if (sceneRenderer && target == sceneRenderer->GetEditorTarget()) return true;
    if (!targetObject) return false; // 指定されているが解決できない場合は適用しない
    std::vector<IRenderTarget *> targets;
    SceneRenderer::CollectRenderTargets(targetObject, targets);
    return std::find(targets.begin(), targets.end(), target) != targets.end();
}

/// @brief EditorOnlyオブジェクト（祖先を含む）のコンポーネントを、エディター用以外の描画先から除外するか
/// @details ライト・カメラがEditorOnlyオブジェクトに付いている場合、エディターのシーンビュー以外へは適用しない
inline bool IsExcludedAsEditorOnly(const IObjectComponent *component, const IRenderTarget *target, const SceneRenderer *sceneRenderer) {
    if (!component || !sceneRenderer) return false;
    if (target == sceneRenderer->GetEditorTarget()) return false;
    const EmptyObject *owner = component->GetOwnerObject();
    return owner && owner->IsEditorOnlyInHierarchy();
}

/// @brief 指定の描画先・パイプラインに適用されるPoint/Spot/Directionalライトを収集する
/// @details BindLightBuffersAndShadowMapとProcessLightCullingの両方から使われる（ライトの絞り込み条件は完全に一致させる）
/// @param findShadowIndex ライトが影を生成する場合のシャドウマップスロット番号を返すコールバック（呼び出し側が用意する）
inline void CollectLightsForTarget(SceneRenderer *sceneRenderer, IRenderTarget *target, const std::string &pipelineName,
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
        if (!IsTargetMatch(lightRenderer->GetTargetObject(), lightRenderer->GetTargetObjectID().IsValid(), target, sceneRenderer)) continue;
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
inline ConstantBufferResource *ResolveCameraConstantBuffer(SceneRenderer *sceneRenderer, IRenderTarget *target, const std::string &pipelineName) {
    if (auto *editorCameraBuffer = sceneRenderer->GetEditorCameraBuffer(target)) {
        return editorCameraBuffer;
    }
    ConstantBufferResource *result = nullptr;
    for (auto *cameraRenderer : sceneRenderer->GetCameraRenderers()) {
        if (!cameraRenderer || !cameraRenderer->IsActive()) continue;
        if (IsExcludedAsEditorOnly(cameraRenderer, target, sceneRenderer)) continue;
        if (!cameraRenderer->GetPipelineName().empty() && cameraRenderer->GetPipelineName() != pipelineName) continue;
        if (!IsTargetMatch(cameraRenderer->GetTargetObject(), cameraRenderer->GetTargetObjectID().IsValid(), target, sceneRenderer)) continue;
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
inline IPostProcessComponent::CameraInfo ResolveCameraInfoForPostProcess(SceneRenderer *sceneRenderer, IRenderTarget *target) {
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
        if (!IsTargetMatch(cameraRenderer->GetTargetObject(), cameraRenderer->GetTargetObjectID().IsValid(), target, sceneRenderer)) continue;
        if (!cameraRenderer->IsRenderTargetIncluded(target)) continue;
        result.valid = true;
        result.viewProjection = cameraRenderer->GetViewProjectionMatrix();
        result.worldPosition = cameraRenderer->GetWorldPosition();
        result.nearClip = cameraRenderer->GetNearClip();
        result.farClip = cameraRenderer->GetFarClip();
    }
    return result;
}

} // namespace KashipanEngine::RendererInternal
