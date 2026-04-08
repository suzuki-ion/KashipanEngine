#include "FullScreenTriangle.hlsli"
#include "VignetteCB.hlsli"

Texture2D gTexture : register(t0);
SamplerState gSampler : register(s0);

float4 main(VSOutput input) : SV_Target0 {
    const float2 uv = saturate(input.uv);
    const float4 baseColor = gTexture.Sample(gSampler, uv);

    const float dist = length(uv - gVignetteCenter);
    const float edge = smoothstep(gVignetteInnerRadius, gVignetteInnerRadius + gVignetteSmoothness, dist);
    const float vignette = saturate(edge * gVignetteIntensity);

    const float3 outColor = lerp(baseColor.rgb, gVignetteColor.rgb, vignette);
    return float4(outColor, baseColor.a);
}
