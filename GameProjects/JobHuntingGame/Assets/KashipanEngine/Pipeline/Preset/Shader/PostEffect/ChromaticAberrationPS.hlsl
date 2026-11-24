#include "FullScreenTriangle.hlsli"

Texture2D gTexture : register(t0);
SamplerState gSampler : register(s0);

cbuffer ChromaticAberrationCB : register(b0)
{
    float2 gDirection; // おおむね正規化された画面上の方向（例: (1, 0)）
    float  gStrength;  // UV オフセットの大きさ
};

float4 main(VSOutput input) : SV_Target0 {
    float2 uv = input.uv;
    float2 off = gDirection * gStrength;

    float r = gTexture.Sample(gSampler, uv + off).r;
    float4 centerColor = gTexture.Sample(gSampler, uv);
    float g = centerColor.g;
    float b = gTexture.Sample(gSampler, uv - off).b;

    return float4(r, g, b, centerColor.a);
}
