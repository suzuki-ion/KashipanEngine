#include "FullScreenTriangle.hlsli"
#include "OutlineCB.hlsli"

Texture2D gTexture : register(t0);
Texture2D gDepthTexture : register(t1);
SamplerState gSampler : register(s0);
SamplerState gDepthSampler : register(s1);

float LinearizeDepth(float nonLinearDepth) {
    // Z_view = (Near * Far) / (Far - Z_ndc * (Far - Near))
	return (gNearClip * gFarClip) / (gFarClip - nonLinearDepth * (gFarClip - gNearClip));
}

float4 main(VSOutput input) : SV_Target0 {
	float rawCenterDepth = gDepthTexture.Sample(gDepthSampler, input.uv);
	float linearCenterDepth = LinearizeDepth(rawCenterDepth);
	
    if (rawCenterDepth >= 1.0f) return gTexture.Sample(gSampler, input.uv);

	float2 offset = gTexelSize * gOutlineThickness;
	
	float rawN = gDepthTexture.Sample(gDepthSampler, input.uv + float2(0, -offset.y));
	float rawS = gDepthTexture.Sample(gDepthSampler, input.uv + float2(0, offset.y));
	float rawE = gDepthTexture.Sample(gDepthSampler, input.uv + float2(offset.x, 0));
	float rawW = gDepthTexture.Sample(gDepthSampler, input.uv + float2(-offset.x, 0));

	float depthN = LinearizeDepth(rawN);
	float depthS = LinearizeDepth(rawS);
	float depthE = LinearizeDepth(rawE);
	float depthW = LinearizeDepth(rawW);
	
	float diffN = abs(linearCenterDepth - depthN);
	float diffS = abs(linearCenterDepth - depthS);
	float diffE = abs(linearCenterDepth - depthE);
	float diffW = abs(linearCenterDepth - depthW);
	
	float maxDiff = max(max(diffN, diffS), max(diffE, diffW));
	
	float4 color = gTexture.Sample(gSampler, input.uv);
	
	if (maxDiff > gDepthThreshold) {
		return gOutlineColor;
	}

	return color;
}