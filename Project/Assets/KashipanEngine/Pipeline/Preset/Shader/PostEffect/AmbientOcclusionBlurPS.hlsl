// AOのノイズ除去用ブラー（深度考慮版）。物体の境界をまたいで平滑化しないよう、
// 中心と周辺サンプルの線形深度差がしきい値を超えるサンプルは重み0で除外する
#include "FullScreenTriangle.hlsli"
#include "AmbientOcclusionBlurCB.hlsli"

Texture2D gTexture : register(t0);
Texture2D gDepthTexture : register(t1);
SamplerState gSampler : register(s0);
SamplerState gDepthSampler : register(s1);

float LinearizeDepth(float nonLinearDepth) {
	return (gNearClip * gFarClip) / (gFarClip - nonLinearDepth * (gFarClip - gNearClip));
}

float4 main(VSOutput input) : SV_Target0 {
	float centerRawDepth = gDepthTexture.Sample(gDepthSampler, input.uv).r;
	if (centerRawDepth >= 1.0f) {
		return gTexture.Sample(gSampler, input.uv);
	}
	float centerLinearDepth = LinearizeDepth(centerRawDepth);

	float sum = 0.0f;
	float wsum = 0.0f;
	for (int y = -gRadius; y <= gRadius; ++y) {
		for (int x = -gRadius; x <= gRadius; ++x) {
			float2 offset = float2(x, y) * gTexelSize;
			float2 uv = input.uv + offset;
			float rawDepth = gDepthTexture.Sample(gDepthSampler, uv).r;
			if (rawDepth >= 1.0f) {
				continue;
			}
			float linearDepth = LinearizeDepth(rawDepth);
			if (abs(linearDepth - centerLinearDepth) > gDepthThreshold) {
				continue;
			}
			float spatialWeight = exp(-float(x * x + y * y) / max(1.0f, float(gRadius * gRadius)));
			float ao = gTexture.Sample(gSampler, uv).r;
			sum += ao * spatialWeight;
			wsum += spatialWeight;
		}
	}

	float result = (wsum > 0.0f) ? (sum / wsum) : gTexture.Sample(gSampler, input.uv).r;
	return float4(result, result, result, 1.0f);
}
