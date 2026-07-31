#include "FullScreenTriangle.hlsli"
#include "ColorAdjustCB.hlsli"

Texture2D gTexture : register(t0);
SamplerState gSampler : register(s0);

float3 ApplyTemperature(float3 color, float temperature) {
	float warm = saturate(temperature);
	float cool = saturate(-temperature);
	float3 warmTint = float3(1.0 + warm * 0.1, 1.0, 1.0 - warm * 0.1);
	float3 coolTint = float3(1.0 - cool * 0.1, 1.0, 1.0 + cool * 0.1);
	return color * warmTint * coolTint;
}

float3 ApplyContrast(float3 color, float contrast) {
	return (color - 0.5f) * contrast + 0.5f;
}

float3 ApplySaturation(float3 color, float saturation) {
	float lum = dot(color, float3(0.2126, 0.7152, 0.0722));
	return lerp(float3(lum, lum, lum), color, saturation);
}

float4 main(VSOutput input) : SV_Target0 {
	float2 uv = saturate(input.uv);
	float4 baseColor = gTexture.Sample(gSampler, uv);

	float3 color = baseColor.rgb + gBrightness;
	color = ApplyContrast(color, gContrast);
	color = ApplySaturation(color, gSaturation);
	color = ApplyTemperature(color, gTemperature);
	color += gColorBalance;

	return float4(saturate(color), baseColor.a);
}
