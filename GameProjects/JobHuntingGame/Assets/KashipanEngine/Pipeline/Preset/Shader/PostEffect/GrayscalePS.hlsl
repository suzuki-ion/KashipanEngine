#include "FullScreenTriangle.hlsli"

Texture2D gTexture : register(t0);
SamplerState gSampler : register(s0);

cbuffer GrayscaleCB : register(b0) {
	float gIntensity;
	float3 gPad;
};

float3 ToGrayscale(float3 color) {
	float lum = dot(color, float3(0.2125, 0.7154, 0.0721));
	return float3(lum, lum, lum);
}

float4 main(VSOutput input) : SV_Target0 {
	float2 uv = saturate(input.uv);
	float4 baseColor = gTexture.Sample(gSampler, uv);
	float3 gray = ToGrayscale(baseColor.rgb);
	float intensity = saturate(gIntensity);
	float3 outColor = lerp(baseColor.rgb, gray, intensity);
	return float4(outColor, baseColor.a);
}
