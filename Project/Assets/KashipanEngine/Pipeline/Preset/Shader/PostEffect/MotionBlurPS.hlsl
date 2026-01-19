#include "FullScreenTriangle.hlsli"
#include "MotionBlurCB.hlsli"

Texture2D gSceneTexture : register(t0);
Texture2D gVelocityTexture : register(t1);
SamplerState gSampler : register(s0);

float4 main(VSOutput input) : SV_Target0 {
	float2 uv = input.uv;
	float3 cur = gSceneTexture.Sample(gSampler, uv).rgb;

	float2 velEncoded = gVelocityTexture.Sample(gSampler, uv).xy;
	float2 velUv = velEncoded * 2.0f - 1.0f;
	
	// Convert UV delta to pixel delta.
	float2 velPixels = velUv / gInvResolution;
	float speedPixels = length(velPixels);

	float blurPixels = min(speedPixels * gVelocityScale, gMaxBlurPixels);
	blurPixels *= gIntensity;

	uint samples = max(gSamples, 1u);
	if (blurPixels <= 0.1 || samples <= 1u) {
		return float4(cur, 1.0);
	}

	float2 dir = (speedPixels > 1e-6) ? (velPixels / speedPixels) : float2(0.0, 0.0);
	float2 deltaUv = dir * (blurPixels * gInvResolution);

	float3 accum = 0.0;
	float wsum = 0.0;

	for (uint i = 0; i < samples; ++i) {
		float t = (samples == 1u) ? 0.0 : (float(i) / float(samples - 1u));
		float o = t * 2.0 - 1.0;
		float2 suv = uv + deltaUv * o;
		float3 s = gSceneTexture.Sample(gSampler, suv).rgb;

		float w = 1.0 - abs(o);
		accum += s * w;
		wsum += w;
	}

	float3 outColor = (wsum > 0.0) ? (accum / wsum) : cur;
	return float4(outColor, 1.0);
}