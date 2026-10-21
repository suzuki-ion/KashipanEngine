#include "FullScreenTriangle.hlsli"

Texture2D gTexture : register(t0);
SamplerState gSampler : register(s0);

cbuffer DitherCB : register(b0)
{
    float2 invPatternResolution; // 1.0 / pattern resolution
    float intensity;      // ディゾルブ進行度（0: 無適用 〜 1: 画面全体が消える）
    uint  color;          // 0: 単色, 1: 色あり
    uint  pad;
};

float luminance(float3 c) {
    return dot(c, float3(0.299, 0.587, 0.114));
}

// 4x4 Bayer matrix (normalized 0..1)
static const float ditherMatrix[16] = {
    0.0/16.0,  8.0/16.0,  2.0/16.0, 10.0/16.0,
    12.0/16.0, 4.0/16.0, 14.0/16.0, 6.0/16.0,
    3.0/16.0, 11.0/16.0, 1.0/16.0, 9.0/16.0,
    15.0/16.0, 7.0/16.0, 13.0/16.0, 5.0/16.0
};

struct VS_OUT {
    float4 pos : SV_POSITION;
    float2 uv  : TEXCOORD0;
};

float4 main(VS_OUT input) : SV_Target0 {
    float2 uv = input.uv;

    // pixel coordinates
    float2 pixelPos = uv / invPatternResolution; // uv * pattern resolution
    int2 ipos = int2(floor(pixelPos));

    // 4x4 matrix indices
    int mx = ipos.x & 3; // mod 4
    int my = ipos.y & 3;
    int idx = my * 4 + mx;
    float threshold = ditherMatrix[idx];

    float4 colorSample = gTexture.Sample(gSampler, uv);

    // preserve fully transparent pixels
    if (colorSample.a <= 0.0f) return colorSample;

    // Bayer行列の閾値を「消えていく順番」として使うディゾルブ表現。
    // intensityが閾値以下のセルはそのまま残り、閾値を超えたセルから抜けていく。
    // intensity=0で全セルの閾値(0/16〜15/16)を下回るため無適用、
    // intensity=1で全セルの閾値を上回るため画面全体が消える
    float mask = step(intensity, threshold);

    bool useColor = (color != 0);
    float3 baseRgb = colorSample.rgb;
    if (!useColor) {
        float lum = luminance(colorSample.rgb);
        baseRgb = float3(lum, lum, lum);
    }

    return float4(baseRgb * mask, colorSample.a * mask);
}
