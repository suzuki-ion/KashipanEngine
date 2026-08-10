#include "FullScreenTriangle.hlsli"
#include "TemporalBlendCB.hlsli"

Texture2D gSceneTexture : register(t0);
Texture2D gHistoryTexture : register(t1);
SamplerState gSampler : register(s0);

float4 main(VSOutput input) : SV_Target0 {
    float4 current = gSceneTexture.Sample(gSampler, input.uv);
    float4 history = gHistoryTexture.Sample(gSampler, input.uv);
    return lerp(current, history, gHistoryWeight);
}
