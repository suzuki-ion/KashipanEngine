// Forward+ のタイルライトカリング用Computeシェーダー
// 画面を16x16ピクセルのタイルへ分割し、タイルごとに（Directional以外の）全種別のライトの
// ビュー空間バウンディングボックスをスクリーン空間の矩形へ投影してタイルの矩形と重なるかを判定する
// （深度プリパスを使わないため、近/遠平面での正確なクリップは行わず保守的に判定する）。
// ヒットしたライトのインデックスを gTileLightIndices の該当タイル領域へ書き込む（先頭にヒット数、
// 続けてインデックス列。上位3bitがライト種別タグ、下位29bitが配列インデックス。ObjectPS.hlslの
// LIGHT_TAG_* と一致させること）。

#include "../Object/Object3D.hlsli"

#define LIGHT_TAG_POINT  0u
#define LIGHT_TAG_SPOT   1u
#define LIGHT_TAG_SPHERE 2u
#define LIGHT_TAG_DISC   3u
#define LIGHT_TAG_RECT   4u
#define LIGHT_TAG_TUBE   5u
#define LIGHT_TAG_BOX    6u
#define LIGHT_TAG_SHIFT  29u

cbuffer TileCullingConstants : register(b1)
{
    float2 gScreenSize;
    uint gTileCountX;
    uint gTileCountY;
    uint gPointLightCount;
    uint gSpotLightCount;
    uint gSphereLightCount;
    uint gDiscLightCount;
    uint gRectLightCount;
    uint gTubeLightCount;
    uint gBoxLightCount;
    uint gMaxLightsPerTile;
    uint gTileSize;
};

StructuredBuffer<PointLight> gPointLights : register(t0);
StructuredBuffer<SpotLight> gSpotLights : register(t1);
StructuredBuffer<SphereLight> gSphereLights : register(t2);
StructuredBuffer<DiscLight> gDiscLights : register(t3);
StructuredBuffer<RectLight> gRectLights : register(t4);
StructuredBuffer<TubeLight> gTubeLights : register(t5);
StructuredBuffer<BoxLight> gBoxLights : register(t6);
RWStructuredBuffer<uint> gTileLightIndices : register(u0);

// ワールド空間の球（中心・半径）を包含するビュー空間AABBの8頂点をスクリーン空間へ投影し、
// その外接矩形を求める（近/遠平面クリップの近傍・後方はコンサバティブに画面全体を返す）
void ProjectSphereToScreenRect(float3 worldCenter, float radius, out float2 screenMin, out float2 screenMax)
{
    float3 viewCenter = mul(float4(worldCenter, 1.0f), gCamera3D.view).xyz;
    screenMin = float2(1e9f, 1e9f);
    screenMax = float2(-1e9f, -1e9f);
    bool coversScreen = false;

    [unroll]
    for (int i = 0; i < 8; ++i)
    {
        float3 corner = viewCenter + float3(
            (i & 1) ? radius : -radius,
            (i & 2) ? radius : -radius,
            (i & 4) ? radius : -radius);
        float4 clip = mul(float4(corner, 1.0f), gCamera3D.projection);
        if (clip.w <= 0.0001f)
        {
            coversScreen = true;
            continue;
        }
        float2 ndc = clip.xy / clip.w;
        float2 screenPos = float2(
            (ndc.x * 0.5f + 0.5f) * gScreenSize.x,
            (1.0f - (ndc.y * 0.5f + 0.5f)) * gScreenSize.y);
        screenMin = min(screenMin, screenPos);
        screenMax = max(screenMax, screenPos);
    }

    if (coversScreen)
    {
        screenMin = float2(0.0f, 0.0f);
        screenMax = gScreenSize;
    }
}

// Each 8x8 group shares one projection per light, instead of recomputing
// the eight projected corners separately in all 64 tile threads.
groupshared float4 sRects[64];
groupshared uint sTags[64];

uint LoadBounds(uint index, out float3 center, out float radius)
{
    center = 0; radius = 0;
    if (index < gPointLightCount) {
        PointLight light = gPointLights[index];
        center = light.position; radius = light.radius;
        return light.enabled ? (0u << LIGHT_TAG_SHIFT) | index : 0xffffffffu;
    }
    index -= gPointLightCount;
    if (index < gSpotLightCount) {
        SpotLight light = gSpotLights[index];
        center = light.position; radius = light.distance;
        return light.enabled ? (1u << LIGHT_TAG_SHIFT) | index : 0xffffffffu;
    }
    index -= gSpotLightCount;
    if (index < gSphereLightCount) {
        SphereLight light = gSphereLights[index];
        center = light.position; radius = light.radius;
        return light.enabled ? (2u << LIGHT_TAG_SHIFT) | index : 0xffffffffu;
    }
    index -= gSphereLightCount;
    if (index < gDiscLightCount) {
        DiscLight light = gDiscLights[index];
        center = light.position; radius = light.distance;
        return light.enabled ? (3u << LIGHT_TAG_SHIFT) | index : 0xffffffffu;
    }
    index -= gDiscLightCount;
    if (index < gRectLightCount) {
        RectLight light = gRectLights[index];
        center = light.position; radius = light.distance + max(light.width, light.height) * 0.5f;
        return light.enabled ? (4u << LIGHT_TAG_SHIFT) | index : 0xffffffffu;
    }
    index -= gRectLightCount;
    if (index < gTubeLightCount) {
        TubeLight light = gTubeLights[index];
        center = (light.p0 + light.p1) * 0.5f; radius = light.radius + length(light.p1 - light.p0) * 0.5f;
        return light.enabled ? (5u << LIGHT_TAG_SHIFT) | index : 0xffffffffu;
    }
    index -= gTubeLightCount;
    if (index < gBoxLightCount) {
        BoxLight light = gBoxLights[index];
        center = light.position; radius = light.radius + length(float3(light.halfWidth, light.halfHeight, light.halfDepth));
        return light.enabled ? (6u << LIGHT_TAG_SHIFT) | index : 0xffffffffu;
    }
    index -= gBoxLightCount;
    return 0xffffffffu;
}

[numthreads(8, 8, 1)]
void main(uint3 id : SV_DispatchThreadID, uint lane : SV_GroupIndex)
{
    bool valid = id.x < gTileCountX && id.y < gTileCountY;
    uint tileBase = (id.y * gTileCountX + id.x) * (1 + gMaxLightsPerTile);
    float2 tileMin = float2(id.xy * gTileSize);
    float2 tileMax = min(float2((id.xy + 1) * gTileSize), gScreenSize);
    uint total = gPointLightCount + gSpotLightCount + gSphereLightCount + gDiscLightCount + gRectLightCount + gTubeLightCount + gBoxLightCount;
    uint count = 0;
    for (uint base = 0; base < total; base += 64) {
        float3 center; float radius;
        uint tag = base + lane < total ? LoadBounds(base + lane, center, radius) : 0xffffffffu;
        float2 lo = 0, hi = 0;
        if (tag != 0xffffffffu) ProjectSphereToScreenRect(center, radius, lo, hi);
        sRects[lane] = float4(lo, hi); sTags[lane] = tag;
        GroupMemoryBarrierWithGroupSync();
        // Out-of-screen and already-full tiles still participate in BOTH barriers.
        // The light order and truncation at gMaxLightsPerTile match the old shader.
        if (valid) {
            for (uint j = 0; j < min(64u, total - base) && count < gMaxLightsPerTile; ++j) {
                float4 rect = sRects[j];
                if (sTags[j] == 0xffffffffu || rect.z < tileMin.x || rect.x > tileMax.x || rect.w < tileMin.y || rect.y > tileMax.y) continue;
                gTileLightIndices[tileBase + 1 + count++] = sTags[j];
            }
        }
        GroupMemoryBarrierWithGroupSync();
    }
    if (valid) gTileLightIndices[tileBase] = count;
}
