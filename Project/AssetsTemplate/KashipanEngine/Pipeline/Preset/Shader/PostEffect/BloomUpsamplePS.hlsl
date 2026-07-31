#include "FullScreenTriangle.hlsli"

Texture2D gLowTexture  : register(t0);
Texture2D gHighTexture : register(t1);
SamplerState gSampler : register(s0);

float3 SampleLow(float2 uv) {
    return gLowTexture.Sample(gSampler, uv).rgb;
}

float4 main(VSOutput input) : SV_Target0 {
    float3 high = gHighTexture.Sample(gSampler, input.uv).rgb;
    float3 low = SampleLow(input.uv);
    
    float3 outColor = high + low;
    return float4(outColor, 1.0);
}
