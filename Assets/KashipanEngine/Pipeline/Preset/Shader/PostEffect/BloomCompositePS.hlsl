Texture2D gSceneTexture : register(t0);
Texture2D gBloomTexture : register(t1);
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

float4 main(PSIn input) : SV_Target0 {
    float3 scene = gSceneTexture.Sample(gSampler, input.uv).rgb;
    float3 bloom = gBloomTexture.Sample(gSampler, input.uv).rgb;
    float3 outColor = scene + bloom * gIntensity;
    return float4(outColor, 1.0);
}
