#include "FullScreenTriangle.hlsli"
#include "GTAOBlurCB.hlsli"

Texture2D gAOTexture : register(t0);
Texture2D gDepthTexture : register(t1);
SamplerState gAOSampler : register(s0);
SamplerState gDepthSampler : register(s1);

float LinearizeDepth(float depth) {
	return (gNearClip * gFarClip) / max(gFarClip - depth * (gFarClip - gNearClip), 1e-6f);
}

float4 main(VSOutput input) : SV_Target0 {
	float centerRawDepth = gDepthTexture.Sample(gDepthSampler, input.uv).r;
	if (centerRawDepth >= 1.0f) return 1.0f.xxxx;
	float centerDepth = LinearizeDepth(centerRawDepth);

	float sum = 0.0f;
	float weightSum = 0.0f;
	[loop]
	for (int offsetIndex = -gBlurRadius; offsetIndex <= gBlurRadius; ++offsetIndex) {
		float2 sampleUv = input.uv + gBlurDirection * gAOTexelSize * float(offsetIndex);
		float sampleRawDepth = gDepthTexture.Sample(gDepthSampler, sampleUv).r;
		if (sampleRawDepth >= 1.0f) continue;

		float depthDifference = LinearizeDepth(sampleRawDepth) - centerDepth;
		float depthWeight = exp(-(depthDifference * depthDifference) /
			max(2.0f * gDepthThreshold * gDepthThreshold, 1e-6f));
		float sigma = max(float(gBlurRadius) * 0.5f, 1.0f);
		float spatialWeight = exp(-float(offsetIndex * offsetIndex) / (2.0f * sigma * sigma));
		float weight = depthWeight * spatialWeight;
		sum += gAOTexture.Sample(gAOSampler, sampleUv).r * weight;
		weightSum += weight;
	}

	float ao = (weightSum > 0.0f) ? (sum / weightSum) : gAOTexture.Sample(gAOSampler, input.uv).r;
	return float4(ao, ao, ao, 1.0f);
}
