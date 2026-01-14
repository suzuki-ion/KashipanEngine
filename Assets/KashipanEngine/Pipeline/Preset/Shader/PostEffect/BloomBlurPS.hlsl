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

float3 SampleBlur9(float2 uv) {
    uint w, h;
    gTexture.GetDimensions(w, h);
    float2 texel = float2(1.0 / max((float)w, 1.0), 1.0 / max((float)h, 1.0));
    float2 d = texel * gBlurRadius;

    float3 c = 0;
    c += gTexture.Sample(gSampler, uv + d * float2(-1, -1)).rgb;
    c += gTexture.Sample(gSampler, uv + d * float2( 0, -1)).rgb;
    c += gTexture.Sample(gSampler, uv + d * float2( 1, -1)).rgb;
    c += gTexture.Sample(gSampler, uv + d * float2(-1,  0)).rgb;
    c += gTexture.Sample(gSampler, uv).rgb;
    c += gTexture.Sample(gSampler, uv + d * float2( 1,  0)).rgb;
    c += gTexture.Sample(gSampler, uv + d * float2(-1,  1)).rgb;
    c += gTexture.Sample(gSampler, uv + d * float2( 0,  1)).rgb;
    c += gTexture.Sample(gSampler, uv + d * float2( 1,  1)).rgb;

    return c / 9.0;
}

float4 main(PSIn input) : SV_Target0 {
    float3 blur = SampleBlur9(input.uv);
    return float4(blur, 1.0);
}
