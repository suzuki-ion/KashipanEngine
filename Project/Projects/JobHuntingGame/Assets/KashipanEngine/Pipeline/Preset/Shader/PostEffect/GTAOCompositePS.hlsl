#include "FullScreenTriangle.hlsli"
#include "GTAOCompositeCB.hlsli"

Texture2D gSceneTexture : register(t0);
Texture2D gAOTexture : register(t1);
Texture2D gDepthTexture : register(t2);
SamplerState gSceneSampler : register(s0);
SamplerState gAOSampler : register(s1);
SamplerState gDepthSampler : register(s2);

float LinearizeDepth(float depth) {
	return (gNearClip * gFarClip) / max(gFarClip - depth * (gFarClip - gNearClip), 1e-6f);
}

float4 main(VSOutput input) : SV_Target0 {
	float4 sceneColor = gSceneTexture.Sample(gSceneSampler, input.uv);
	float centerRawDepth = gDepthTexture.Sample(gDepthSampler, input.uv).r;
	if (centerRawDepth >= 1.0f) return sceneColor;
	float centerDepth = LinearizeDepth(centerRawDepth);

	float2 halfPosition = input.uv / gAOTexelSize - 0.5f;
	float2 basePosition = floor(halfPosition);
	float2 fraction = frac(halfPosition);
	float aoSum = 0.0f;
	float weightSum = 0.0f;

	[unroll]
	for (uint y = 0; y < 2; ++y) {
		[unroll]
		for (uint x = 0; x < 2; ++x) {
			float2 offset = float2(x, y);
			float2 sampleUv = (basePosition + offset + 0.5f) * gAOTexelSize;
			float sampleRawDepth = gDepthTexture.Sample(gDepthSampler, sampleUv).r;
			if (sampleRawDepth >= 1.0f) continue;

			float2 bilinear = 1.0f - abs(offset - fraction);
			float spatialWeight = bilinear.x * bilinear.y;
			float depthDifference = LinearizeDepth(sampleRawDepth) - centerDepth;
			float depthWeight = exp(-(depthDifference * depthDifference) /
				max(2.0f * gDepthThreshold * gDepthThreshold, 1e-6f));
			float weight = spatialWeight * depthWeight;
			aoSum += gAOTexture.Sample(gAOSampler, sampleUv).r * weight;
			weightSum += weight;
		}
	}

	float ao = (weightSum > 1e-5f) ? (aoSum / weightSum) : gAOTexture.Sample(gAOSampler, input.uv).r;
	if (gShowAOOnly != 0u) return float4(ao, ao, ao, 1.0f);
	return float4(sceneColor.rgb * ao, sceneColor.a);
}
