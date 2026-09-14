#include "FullScreenTriangle.hlsli"
#include "ScanlineCB.hlsli"

Texture2D gTexture : register(t0);
SamplerState gSampler : register(s0);

float4 main(VSOutput input) : SV_Target0 {
    float2 uv = saturate(input.uv);
    float4 baseColor = gTexture.Sample(gSampler, uv);

    float spacing = max(gLineSpacing, 1.0f);
    float pixelY = uv.y / gInvResolution.y;
    float cycle = frac(pixelY / spacing);

    float aa = 1.0f / spacing;
    float darkMask = 1.0f - smoothstep(gThickness - aa, gThickness + aa, cycle);

    float3 outColor = baseColor.rgb * (1.0f - darkMask * saturate(gIntensity));
    return float4(outColor, baseColor.a);
}
