#include "FullScreenTriangle.hlsli"

Texture2D gTexture : register(t0);
SamplerState gSampler : register(s0);

cbuffer ChromaticAberrationCB : register(b0)
{
    float2 gDirection; // normalized-ish screen direction (e.g. (1,0))
    float  gStrength;  // UV offset magnitude
};

float4 main(VSOutput input) : SV_Target0 {
    float2 uv = input.uv;
    float2 off = gDirection * gStrength;

    float r = gTexture.Sample(gSampler, uv + off).r;
    float g = gTexture.Sample(gSampler, uv).g;
    float b = gTexture.Sample(gSampler, uv - off).b;

    return float4(r, g, b, 1.0);
}
