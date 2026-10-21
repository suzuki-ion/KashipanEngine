#include "FullScreenTriangle.hlsli"
#include "BloomCB.hlsli"

Texture2D gSceneTexture : register(t0);
Texture2D gBloomTexture : register(t1);
SamplerState gSampler : register(s0);

float4 main(VSOutput input) : SV_Target0 {
    float4 scene = gSceneTexture.Sample(gSampler, input.uv);
    float3 bloom = gBloomTexture.Sample(gSampler, input.uv).rgb;
    float3 outColor = scene.rgb + bloom * gIntensity;
    return float4(outColor, scene.a);
}
