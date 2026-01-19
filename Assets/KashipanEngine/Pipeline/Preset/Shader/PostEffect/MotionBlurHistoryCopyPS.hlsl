#include "FullScreenTriangle.hlsli"

Texture2D gSceneTexture : register(t0);
SamplerState gSampler : register(s0);

float4 main(VSOutput input) : SV_Target0 {
    return gSceneTexture.Sample(gSampler, input.uv);
}
