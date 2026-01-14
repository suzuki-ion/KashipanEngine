Texture2D gTexture : register(t0);
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

static const float3 kLuma = float3(0.2126, 0.7152, 0.0722);

float3 Prefilter(float3 c) {
    float l = dot(c, kLuma);

    float knee = gThreshold * gSoftKnee;
    float soft = l - (gThreshold - knee);
    soft = saturate(soft / max(knee * 2.0, 1e-5));
    float contrib = max(l - gThreshold, 0.0) + soft * soft * knee;

    return c * (contrib / max(l, 1e-5));
}

float4 main(PSIn input) : SV_Target0 {
    float3 src = gTexture.Sample(gSampler, input.uv).rgb;
    float3 bright = Prefilter(src);
    return float4(bright, 1.0);
}
