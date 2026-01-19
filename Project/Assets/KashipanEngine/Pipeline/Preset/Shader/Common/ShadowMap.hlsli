#pragma once

cbuffer ShadowMapConstants : register(b10) {
    float4x4 gLightViewProjection;
	float gLightNear;
	float gLightFar;
};

Texture2D gShadowMap : register(t2);
SamplerState gShadowSampler : register(s1);

inline float ComputeShadowFactor(float3 worldPos) {
	float4 lightClip = mul(float4(worldPos, 1.0f), gLightViewProjection);
	float3 ndc = lightClip.xyz / lightClip.w;
	float2 uv;
	uv.x = ndc.x * 0.5f + 0.5f;
	uv.y = -ndc.y * 0.5f + 0.5f;

    if (uv.x < 0.0f || uv.x > 1.0f || uv.y < 0.0f || uv.y > 1.0f || ndc.z < 0.0f || ndc.z > 1.0f) {
		return 1.0f;
    }
	
	float depthFromLight = ndc.z;
	float shadowDepth = gShadowMap.Sample(gShadowSampler, uv).r;
	float bias = 0.001f;
	return depthFromLight <= shadowDepth + bias ? 1.0f : 0.35f;
}
