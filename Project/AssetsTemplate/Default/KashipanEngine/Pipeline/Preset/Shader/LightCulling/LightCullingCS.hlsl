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
        gTileLightIndices[tileBase + 1 + count] = (LIGHT_TAG_POINT << LIGHT_TAG_SHIFT) | p;
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
        gTileLightIndices[tileBase + 1 + count] = (LIGHT_TAG_SPOT << LIGHT_TAG_SHIFT) | s;
        ++count;
    }

    for (uint sp = 0; sp < gSphereLightCount && count < gMaxLightsPerTile; ++sp)
    {
        SphereLight light = gSphereLights[sp];
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
        gTileLightIndices[tileBase + 1 + count] = (LIGHT_TAG_SPHERE << LIGHT_TAG_SHIFT) | sp;
        ++count;
    }

    for (uint d = 0; d < gDiscLightCount && count < gMaxLightsPerTile; ++d)
    {
        DiscLight light = gDiscLights[d];
        if (!light.enabled)
        {
            continue;
        }
        // 片面（半球）発光だが、簡略化のため保守的な包含球（半径distance）で判定する
        float2 lightMin, lightMax;
        ProjectSphereToScreenRect(light.position, light.distance, lightMin, lightMax);
        if (lightMax.x < tileMin.x || lightMin.x > tileMax.x || lightMax.y < tileMin.y || lightMin.y > tileMax.y)
        {
            continue;
        }
        gTileLightIndices[tileBase + 1 + count] = (LIGHT_TAG_DISC << LIGHT_TAG_SHIFT) | d;
        ++count;
    }

    for (uint r = 0; r < gRectLightCount && count < gMaxLightsPerTile; ++r)
    {
        RectLight light = gRectLights[r];
        if (!light.enabled)
        {
            continue;
        }
        // 矩形の対角も包含できるよう、半径distanceへ幅高さの半分を加えた保守的な包含球で判定する
        float boundRadius = light.distance + max(light.width, light.height) * 0.5f;
        float2 lightMin, lightMax;
        ProjectSphereToScreenRect(light.position, boundRadius, lightMin, lightMax);
        if (lightMax.x < tileMin.x || lightMin.x > tileMax.x || lightMax.y < tileMin.y || lightMin.y > tileMax.y)
        {
            continue;
        }
        gTileLightIndices[tileBase + 1 + count] = (LIGHT_TAG_RECT << LIGHT_TAG_SHIFT) | r;
        ++count;
    }

    for (uint tb = 0; tb < gTubeLightCount && count < gMaxLightsPerTile; ++tb)
    {
        TubeLight light = gTubeLights[tb];
        if (!light.enabled)
        {
            continue;
        }
        // チューブの両端点を包含できるよう、中点・半径(radius + チューブ半長)の保守的な包含球で判定する
        float3 center = (light.p0 + light.p1) * 0.5f;
        float halfLength = length(light.p1 - light.p0) * 0.5f;
        float boundRadius = light.radius + halfLength;
        float2 lightMin, lightMax;
        ProjectSphereToScreenRect(center, boundRadius, lightMin, lightMax);
        if (lightMax.x < tileMin.x || lightMin.x > tileMax.x || lightMax.y < tileMin.y || lightMin.y > tileMax.y)
        {
            continue;
        }
        gTileLightIndices[tileBase + 1 + count] = (LIGHT_TAG_TUBE << LIGHT_TAG_SHIFT) | tb;
        ++count;
    }

    for (uint bx = 0; bx < gBoxLightCount && count < gMaxLightsPerTile; ++bx)
    {
        BoxLight light = gBoxLights[bx];
        if (!light.enabled)
        {
            continue;
        }
        // ボックスの半対角（半幅・半高・半奥行きの合成長）に減衰範囲radiusを加えた保守的な包含球で判定する
        float boundRadius = light.radius + length(float3(light.halfWidth, light.halfHeight, light.halfDepth));
        float2 lightMin, lightMax;
        ProjectSphereToScreenRect(light.position, boundRadius, lightMin, lightMax);
        if (lightMax.x < tileMin.x || lightMin.x > tileMax.x || lightMax.y < tileMin.y || lightMin.y > tileMax.y)
        {
            continue;
        }
        gTileLightIndices[tileBase + 1 + count] = (LIGHT_TAG_BOX << LIGHT_TAG_SHIFT) | bx;
        ++count;
    }

    gTileLightIndices[tileBase] = count;
}
