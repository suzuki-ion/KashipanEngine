#include "FullScreenTriangle.hlsli"
#include "BarrelDistortionCB.hlsli"

Texture2D gTexture : register(t0);
SamplerState gSampler : register(s0);

float4 main(VSOutput input) : SV_Target0 {
    float2 uv = input.uv;

    float2 cc = (uv - 0.5f) * max(gZoom, 0.0001f);
    float r2 = dot(cc, cc);
    float2 distortedUV = cc * (1.0f + gStrength * r2) + 0.5f;

    if (any(distortedUV < 0.0f) || any(distortedUV > 1.0f)) {
        return gEdgeColor;
    }

    return gTexture.Sample(gSampler, distortedUV);
}
