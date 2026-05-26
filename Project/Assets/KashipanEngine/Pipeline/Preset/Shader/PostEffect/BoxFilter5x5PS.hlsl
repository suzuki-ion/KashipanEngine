#include "FullScreenTriangle.hlsli"

Texture2D gTexture : register(t0);
SamplerState gSampler : register(s0);

cbuffer BoxFilter5x5CB : register(b0) {
	float2 gInvResolution;
	float gIntensity;
	float gPad;
};

float4 main(VSOutput input) : SV_Target0 {
	float2 uv = saturate(input.uv);
	float3 sum = 0.0f;

	[unroll]
	for (int y = -2; y <= 2; ++y) {
		[unroll]
		for (int x = -2; x <= 2; ++x) {
			float2 offset = float2(x, y) * gInvResolution;
			sum += gTexture.Sample(gSampler, uv + offset).rgb;
		}
	}

	float3 filtered = sum / 25.0f;
	float3 baseColor = gTexture.Sample(gSampler, uv).rgb;
	float intensity = saturate(gIntensity);
	float3 outColor = lerp(baseColor, filtered, intensity);
	return float4(outColor, 1.0f);
}
