// Forward+ のタイルライトカリング用Computeシェーダー
// 画面を16x16ピクセルのタイルへ分割し、タイルごとにPoint/Spotライトのビュー空間バウンディングボックスを
// スクリーン空間の矩形へ投影してタイルの矩形と重なるかを判定する（深度プリパスを使わないため、
// 近/遠平面での正確なクリップは行わず保守的に判定する）。ヒットしたライトのインデックスを
// gTileLightIndices の該当タイル領域へ書き込む（先頭にヒット数、続けてインデックス列。
// Spotライトのインデックスは最上位ビットを立てて区別する）。

#include "../Object/Object3D.hlsli"

cbuffer TileCullingConstants : register(b1)
{
    float2 gScreenSize;
    uint gTileCountX;
    uint gTileCountY;
    uint gPointLightCount;
    uint gSpotLightCount;
    uint gMaxLightsPerTile;
    uint gTileSize;
};

StructuredBuffer<PointLight> gPointLights : register(t0);
StructuredBuffer<SpotLight> gSpotLights : register(t1);
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

[numthreads(8, 8, 1)]
void main(uint3 dispatchThreadID : SV_DispatchThreadID)
{
    uint tileX = dispatchThreadID.x;
    uint tileY = dispatchThreadID.y;
    if (tileX >= gTileCountX || tileY >= gTileCountY)
    {
        return;
    }

    uint tileIndex = tileY * gTileCountX + tileX;
    uint tileBase = tileIndex * (1 + gMaxLightsPerTile);

    float2 tileMin = float2(tileX * gTileSize, tileY * gTileSize);
    float2 tileMax = float2(
        min((tileX + 1) * gTileSize, (uint) gScreenSize.x),
        min((tileY + 1) * gTileSize, (uint) gScreenSize.y));

    uint count = 0;

    for (uint p = 0; p < gPointLightCount && count < gMaxLightsPerTile; ++p)
    {
        PointLight light = gPointLights[p];
        if (!light.enabled)
        {
            continue;
        }
        float2 lightMin, lightMax;
        ProjectSphereToScreenRect(light.position, light.radius, lightMin, lightMax);
        if (lightMax.x < tileMin.x || lightMin.x > tileMax.x || lightMax.y < tileMin.y || lightMin.y > tileMax.y)
        {
            continue;
        }
        gTileLightIndices[tileBase + 1 + count] = p;
        ++count;
    }

    for (uint s = 0; s < gSpotLightCount && count < gMaxLightsPerTile; ++s)
    {
        SpotLight light = gSpotLights[s];
        if (!light.enabled)
        {
            continue;
        }
        // 簡略化のため、頂点中心・半径distanceの保守的な包含球を円錐全体の代わりに使う
        float2 lightMin, lightMax;
        ProjectSphereToScreenRect(light.position, light.distance, lightMin, lightMax);
        if (lightMax.x < tileMin.x || lightMin.x > tileMax.x || lightMax.y < tileMin.y || lightMin.y > tileMax.y)
        {
            continue;
        }
        gTileLightIndices[tileBase + 1 + count] = s | 0x80000000;
        ++count;
    }

    gTileLightIndices[tileBase] = count;
}
