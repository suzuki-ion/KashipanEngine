#include "FullScreenTriangle.hlsli"
#include "BloomCB.hlsli"

Texture2D gTexture : register(t0);
SamplerState gSampler : register(s0);

float3 Downsample4Tap(float2 uv) {
    uint w, h;
    gTexture.GetDimensions(w, h);
    float2 texel = float2(1.0 / max((float)w, 1.0), 1.0 / max((float)h, 1.0));

    // 4-tap box filter
    float3 c = 0;
    c += gTexture.Sample(gSampler, uv + texel * float2(-0.5, -0.5)).rgb;
    c += gTexture.Sample(gSampler, uv + texel * float2( 0.5, -0.5)).rgb;
    c += gTexture.Sample(gSampler, uv + texel * float2(-0.5,  0.5)).rgb;
    c += gTexture.Sample(gSampler, uv + texel * float2( 0.5,  0.5)).rgb;
    return c * 0.25;
}

float4 main(VSOutput input) : SV_Target0 {
    float3 c = Downsample4Tap(input.uv);
    return float4(c, 1.0);
}
