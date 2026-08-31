#include "FullScreenTriangle.hlsli"
#include "BloomCB.hlsli"

Texture2D gTexture : register(t0);
SamplerState gSampler : register(s0);

static const float3 kLuma = float3(0.299f, 0.587f, 0.114f);

float3 Prefilter(float3 c) {
	float l = dot(kLuma, c);
    
    float knee = gThreshold * gSoftKnee;
    float soft = l - gThreshold + knee;
    soft = saturate(soft / (knee + 1e-5));
    soft = soft * soft;
    float cutoff = max(soft, step(gThreshold, l));
    
	return c * cutoff;
}

float4 main(VSOutput input) : SV_Target0 {
    float3 src = gTexture.Sample(gSampler, input.uv).rgb;
    float3 bright = Prefilter(src);
    return float4(bright, 1.0);
}
