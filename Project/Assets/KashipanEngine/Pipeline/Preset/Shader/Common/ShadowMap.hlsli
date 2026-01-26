#pragma once

cbuffer ShadowMapConstants : register(b10) {
	float4x4 gLightViewProjection;
	float gLightNear;
	float gLightFar;
};

Texture2D gShadowMap : register(t2);
SamplerComparisonState gShadowSamplerCmp : register(s1);

inline float2 ShadowNdcToUv(float3 ndc) {
	float2 uv;
	uv.x = ndc.x * 0.5f + 0.5f;
	uv.y = -ndc.y * 0.5f + 0.5f;
	return uv;
}

inline bool ShadowIsOutside(float2 uv, float depth) {
	return (uv.x < 0.0f || uv.x > 1.0f || uv.y < 0.0f || uv.y > 1.0f || depth < 0.0f || depth > 1.0f);
}

inline float ShadowPcf3x3(float2 uv, float depthFromLight, float bias) {
	uint w, h;
	gShadowMap.GetDimensions(w, h);
	float2 texel = 1.0f / float2(max(1u, w), max(1u, h));

	float sum = 0.0f;
	for (int y = -1; y <= 1; ++y) {
		for (int x = -1; x <= 1; ++x) {
			float2 o = float2(x, y) * texel;
			sum += gShadowMap.SampleCmpLevelZero(gShadowSamplerCmp, uv + o, depthFromLight - bias);
		}
	}
	return sum / 9.0f;
}

inline float ComputeShadowFactor(float3 worldPos) {
	float4 lightClip = mul(float4(worldPos, 1.0f), gLightViewProjection);
	float3 ndc = lightClip.xyz / lightClip.w;

	float2 uv = ShadowNdcToUv(ndc);
	if (ShadowIsOutside(uv, ndc.z)) {
		return 1.0f;
	}

	float depthFromLight = ndc.z;
	float bias = 0.01f;
	float pcf = ShadowPcf3x3(uv, depthFromLight, bias);
	return lerp(0.5f, 1.0f, saturate(pcf));
}