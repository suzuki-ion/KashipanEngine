Texture2D gLowTexture  : register(t0);
Texture2D gHighTexture : register(t1);
SamplerState gSampler : register(s0);

cbuffer BloomCB : register(b0)
{
    float gThreshold;
    float gSoftKnee;
    float gIntensity;
    float gBlurRadius;
};

struct PSIn {
    float4 pos : SV_Position;
    float2 uv  : TEXCOORD0;
};

float3 SampleLow(float2 uv) {
    // Simple bilinear upsample
    return gLowTexture.Sample(gSampler, uv).rgb;
}

float4 main(PSIn input) : SV_Target0 {
    float3 high = gHighTexture.Sample(gSampler, input.uv).rgb;
    float3 low = SampleLow(input.uv);

    // Accumulate: propagate bloom from lower mip to higher mip
    float3 outColor = high + low;
    return float4(outColor, 1.0);
}
