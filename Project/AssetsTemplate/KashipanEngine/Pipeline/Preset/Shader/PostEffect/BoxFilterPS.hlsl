#include "FullScreenTriangle.hlsli"

Texture2D gTexture : register(t0);
SamplerState gSampler : register(s0);

cbuffer BoxFilterCB : register(b0) {
	float2 gInvResolution;
	float gIntensity;
	float pad;
	int2 halfSize;
};

float4 main(VSOutput input) : SV_Target0 {
	float2 uv = saturate(input.uv);
	float3 sum = 0.0f;
	
	for (int y = -halfSize.y; y <= halfSize.y; ++y) {
		for (int x = -halfSize.x; x <= halfSize.x; ++x) {
			float2 offset = float2(x, y) * gInvResolution;
			sum += gTexture.Sample(gSampler, uv + offset).rgb;
		}
	}

	float3 filtered = sum / (float)( (2 * halfSize.x + 1) * (2 * halfSize.y + 1) );
	float3 baseColor = gTexture.Sample(gSampler, uv).rgb;
	float intensity = saturate(gIntensity);
	float3 outColor = lerp(baseColor, filtered, intensity);
	return float4(outColor, 1.0f);
}
