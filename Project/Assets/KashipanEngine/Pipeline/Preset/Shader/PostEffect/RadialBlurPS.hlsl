#include "FullScreenTriangle.hlsli"
#include "RadialBlurCB.hlsli"

Texture2D gTexture : register(t0);
SamplerState gSampler : register(s0);

float InterleavedGradientNoise(float2 pixelPos) {
    float x = dot(pixelPos, float2(0.06711056, 0.00583715));
    return frac(52.9829189 * frac(x));
}

float GetMaxCornerDistance(float2 center) {
    float d0 = length(center - float2(0.0f, 0.0f));
    float d1 = length(center - float2(1.0f, 0.0f));
    float d2 = length(center - float2(0.0f, 1.0f));
    float d3 = length(center - float2(1.0f, 1.0f));
    return max(max(d0, d1), max(d2, d3));
}

float4 main(VSOutput input) : SV_Target0 {
    float2 uv = input.uv;
    float3 baseColor = gTexture.Sample(gSampler, uv).rgb;

    static const uint kMaxSamples = 64u;
    uint sampleCount = min(max(gSampleCount, 1u), kMaxSamples);
    if (gIntensity <= 0.0f || sampleCount <= 1u) {
        return float4(baseColor, 1.0f);
    }

    float2 toCenter = gRadialCenter - uv;
    float dist = length(toCenter);
    if (dist <= 1e-6f) {
        return float4(baseColor, 1.0f);
    }

    float maxDist = max(GetMaxCornerDistance(gRadialCenter), 1e-6f);
    float normDist = dist / maxDist;

    if (normDist <= gStartRadius) {
        return float4(baseColor, 1.0f);
    }

    float edgeFactor = saturate((normDist - gStartRadius) / max(1e-6f, 1.0f - gStartRadius));
    float2 blurVec = toCenter * (gIntensity * edgeFactor);

    float dither = InterleavedGradientNoise(input.pos.xy) - 0.5f;
    float jitter = dither / float(sampleCount);

    float3 accum = 0.0f;
    float wsum = 0.0f;

    [loop]
    for (uint i = 0u; i < kMaxSamples; ++i) {
        if (i >= sampleCount) {
            break;
        }

        float t = (float(i) + 0.5f) / float(sampleCount);
        t = saturate(t + jitter);

        float shapedT = t * t;
        float2 suv = uv + blurVec * shapedT;
        float3 sampleColor = gTexture.Sample(gSampler, suv).rgb;

        float w = 1.0f - shapedT;
        accum += sampleColor * w;
        wsum += w;
    }

    float3 outColor = (wsum > 0.0f) ? (accum / wsum) : baseColor;
    return float4(outColor, 1.0f);
}