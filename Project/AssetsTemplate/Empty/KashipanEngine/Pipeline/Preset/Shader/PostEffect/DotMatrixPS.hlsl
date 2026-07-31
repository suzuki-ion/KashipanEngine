#include "FullScreenTriangle.hlsli"

Texture2D gTexture : register(t0);
SamplerState gSampler : register(s0);

cbuffer DotMatrixCB : register(b0)
{
    float2 invResolution;   // 1.0 / (width, height)
    float dotSpacing;       // ピクセル単位
    float dotRadius;        // ピクセル単位
    float threshold;        // 0..1（輝度閾値）
    float intensity;        // 出力強度
    uint  monochrome;       // 0/1
    uint  pad0;
};

float luminance(float3 c) {
    return dot(c, float3(0.299, 0.587, 0.114));
}

struct VS_OUT {
    float4 pos : SV_POSITION;
    float2 uv  : TEXCOORD0;
};

float4 main(VS_OUT input) : SV_Target0 {
    float2 uv = input.uv;

    // ピクセル座標（float）
    float2 pixelPos = uv / invResolution; // uv * (width,height)

    // グリッド内でのドット中心を求める
    float2 grid = floor(pixelPos / dotSpacing);
    float2 centerPx = (grid + 0.5) * dotSpacing;
    float2 centerUV = centerPx * invResolution;

    // ドット半径によるマスク（滑らかに）
    float dist = distance(pixelPos, centerPx);
    float mask = smoothstep(dotRadius, max(0.0, dotRadius - 1.0), dist);

    // サンプルは中心位置で行う（ピクセルスナップ）
    float4 color = gTexture.Sample(gSampler, centerUV);

    // 輝度閾値が設定されている場合は明るさでマスク
    if (threshold > 0.0) {
        float lum = luminance(color.rgb);
        float tmask = step(threshold, lum); // 0 or 1
        mask *= tmask;
    }

    // 単色化オプション
    if (monochrome != 0) {
        float l = luminance(color.rgb);
        color.rgb = float3(l, l, l);
    }

    float4 outColor = float4(color.rgb * mask * intensity, color.a * mask);

    return outColor;
}
